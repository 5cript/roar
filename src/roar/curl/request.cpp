#include <roar/curl/request.hpp>

#include <roar/curl/curl_error.hpp>
#include <roar/curl/instance.hpp>
#include <roar/curl/response.hpp>
#include <roar/curl/sink.hpp>
#include <roar/curl/source.hpp>
#include <roar/curl/sources/file_source.hpp>
#include <roar/curl/sources/string_source.hpp>

#include <curl/system.h>
#include <boost/algorithm/string/trim.hpp>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace Roar::Curl
{
    namespace
    {
        static size_t sinksDelegator(
            char* ptr,
            size_t size,
            size_t nmemb,
            std::vector<std::pair<std::function<void(char*, std::size_t)>, std::function<void()>>>* sinks)
        {
            for (auto sink : *sinks)
                (sink.first)(ptr, size * nmemb);
            return size * nmemb;
        }
        static size_t sourceDelegator(char* buffer, size_t size, size_t nmemb, Source* source)
        {
            return source->fetch(buffer, size * nmemb);
        }
        static size_t headerDelegator(
            char* buffer,
            size_t size,
            size_t nitems,
            std::vector<std::function<void(std::string const&, std::string const&)>>* receivers)
        {
            const auto line = std::string{buffer, size * nitems};
            const auto position = line.find_first_of(':');
            if (position != std::string::npos)
            {
                auto value = line.substr(position + 1, line.length() - position - 1);
                boost::algorithm::trim_if(value, [](char c) {
                    return std::isspace(c);
                });
                for (auto const& receiver : *receivers)
                    receiver(line.substr(0, position), value);
            }
            return size * nitems;
        }
        static int
        debugDelegator(CURL*, curl_infotype type, char* data, size_t size, std::vector<VerboseLogEntry>* logs)
        {
            std::string str{data, size};
            logs->push_back({.type = type, .data = str});
            return 0;
        }
    }

    Request::Request()
    {
        setupSinks();
    }
    Request& Request::basicAuth(std::string const& name, std::string const& password)
    {
        check(curl_easy_setopt(instance_, CURLOPT_HTTPAUTH, CURLAUTH_BASIC));
        check(curl_easy_setopt(instance_, CURLOPT_USERNAME, name.c_str()));
        check(curl_easy_setopt(instance_, CURLOPT_PASSWORD, password.c_str()));

        return *this;
    }
    Request& Request::acceptEncoding(std::string const& encoding)
    {
        check(curl_easy_setopt(instance_, CURLOPT_ACCEPT_ENCODING, encoding.c_str()));
        return *this;
    }
    Request& Request::setHeaderFields(std::unordered_map<std::string, std::string> const& fields)
    {
        for (auto const& [key, value] : fields)
            setHeader(key, value);
        return *this;
    }
    Request& Request::setHeader(std::string const& key, std::string const& value)
    {
        headerFields_[key] = value;
        return *this;
    }
    Request& Request::setHeader(boost::beast::http::field key, std::string const& value)
    {
        headerFields_[std::string{to_string(key)}] = value;
        return *this;
    }
    Request& Request::contentType(std::string const& type)
    {
        return setHeader("Content-Type", type);
    }
    Request& Request::verifyPeer(bool verify)
    {
        check(curl_easy_setopt(instance_, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L));
        return *this;
    }
    Request& Request::verifyHost(bool verify)
    {
        check(curl_easy_setopt(instance_, CURLOPT_SSL_VERIFYHOST, verify ? 1L : 0L));
        return *this;
    }
    Response Request::get(std::string const& url)
    {
        this->url(url);
        return perform();
    }
    Request& Request::verbose(bool enable)
    {
        check(curl_easy_setopt(instance_, CURLOPT_VERBOSE, enable ? 1L : 0L));
        return *this;
    }
    Request& Request::verbose(LogsType& logs)
    {
        check(curl_easy_setopt(instance_, CURLOPT_DEBUGDATA, &logs));
        check(curl_easy_setopt(instance_, CURLOPT_DEBUGFUNCTION, debugDelegator));
        return verbose(true);
    }
    Request& Request::connectTimeout(std::chrono::milliseconds timeout)
    {
        check(curl_easy_setopt(instance_, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeout.count())));
        return *this;
    }
    Request& Request::transferTimeout(std::chrono::milliseconds timeout)
    {
        check(curl_easy_setopt(instance_, CURLOPT_TIMEOUT, static_cast<long>(timeout.count())));
        return *this;
    }
    Request& Request::useSignals(bool useSignal)
    {
        check(curl_easy_setopt(instance_, CURLOPT_NOSIGNAL, useSignal ? 0L : 1L));
        return *this;
    }
    Request& Request::onHeaderValue(std::function<void(std::string const&, std::string const&)> const& headerReceiver)
    {
        headerReceivers_.push_back(headerReceiver);
        curl_easy_setopt(instance_, CURLOPT_HEADERDATA, &headerReceivers_);
        curl_easy_setopt(instance_, CURLOPT_HEADERFUNCTION, headerDelegator);
        return *this;
    }
    Request& Request::headerSink(std::unordered_map<std::string, std::string>& headers)
    {
        return onHeaderValue([&headers](std::string const& key, std::string const& value) {
            headers[key] = value;
        });
    }
    Response Request::put(std::string const& url)
    {
        this->url(url);
        if (!source_)
            throw std::runtime_error("You must provide a source to make a put request.");
        check(curl_easy_setopt(instance_, CURLOPT_UPLOAD, 1L));
        return perform();
    }
    Response Request::post(std::string const& url)
    {
        this->url(url);
        check(curl_easy_setopt(instance_, CURLOPT_POST, 1L));

        if (!source_)
        {
            check(curl_easy_setopt(instance_, CURLOPT_POSTFIELDS, ""));
            check(curl_easy_setopt(instance_, CURLOPT_POSTFIELDSIZE, 0L));
        }
        else if (source_ && !source_->isChunked())
        {
            check(curl_easy_setopt(instance_, CURLOPT_POSTFIELDS, nullptr));
            check(curl_easy_setopt(instance_, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(source_->size())));
        }

        return perform();
    }
    Response Request::delete_(std::string const& url)
    {
        this->url(url);
        verb("DELETE");
        return perform();
    }
    Response Request::options(std::string const& url)
    {
        this->url(url);
        verb("OPTIONS");
        return perform();
    }
    Response Request::head(std::string const& url)
    {
        this->url(url);
        verb("HEAD");
        return perform();
    }
    Response Request::patch(std::string const& url)
    {
        this->url(url);
        verb("PATCH");
        return perform();
    }
    Request& Request::verb(std::string const& verb)
    {
        check(curl_easy_setopt(instance_, CURLOPT_CUSTOMREQUEST, verb.c_str()));
        return *this;
    }
    Request& Request::url(std::string const& url)
    {
        check(curl_easy_setopt(instance_, CURLOPT_URL, url.c_str()));
        return *this;
    }
    std::string Request::urlEncode(std::string const& url)
    {
        return std::string{curl_easy_escape(instance_, url.c_str(), static_cast<int>(url.size()))};
    }
#ifdef ROAR_ENABLE_NLOHMANN_JSON
    Request& Request::sink(nlohmann::json& json)
    {
        auto str = std::make_shared<std::string>();
        return sink(
            [str](char const* data, std::size_t count) {
                str->append(data, count);
            },
            [str, &json]() {
                json = nlohmann::json::parse(*str);
            });
    }
#endif
    Request& Request::sink(std::string& str)
    {
        return sink([&str](char const* data, std::size_t count) {
            str.append(data, count);
        });
    }
    Request& Request::sink(std::filesystem::path const& path)
    {
        return sink([file = std::make_shared<std::ofstream>(path, std::ios_base::binary)](
                        char const* data, std::size_t count) mutable {
            file->write(data, static_cast<std::streamsize>(count));
        });
    }
    Request& Request::sink(Sink& customSink)
    {
        return sink([&customSink](char const* data, std::size_t count) {
            customSink.feed(data, count);
        });
    }
    Request& Request::addLibcurlDefaultSink()
    {
        return sink([](char const* data, std::size_t size) {
            fwrite(data, 1, size, stdout);
        });
    }
    Request& Request::source(char const* str)
    {
        return source(std::string{str});
    }
    Request& Request::source(std::string str)
    {
        source_ = std::make_unique<StringSource>(std::move(str));
        setupSource(false);
        return *this;
    }
    Request& Request::source(std::filesystem::path path)
    {
        source_ = std::make_unique<FileSource>(std::move(path));
        setupSource(false);
        return *this;
    }
    void Request::setupSource(bool chunked)
    {
        check(curl_easy_setopt(instance_, CURLOPT_READDATA, source_.get()));
        check(curl_easy_setopt(instance_, CURLOPT_READFUNCTION, sourceDelegator));
        if (!chunked)
            check(curl_easy_setopt(instance_, CURLOPT_INFILESIZE_LARGE, source_->size()));
        else
            headerFields_["Transfer-Encoding"] = "chunked";
    }
    void Request::setupSinks()
    {
        check(curl_easy_setopt(instance_, CURLOPT_WRITEDATA, &sinks_));
        check(curl_easy_setopt(instance_, CURLOPT_WRITEFUNCTION, sinksDelegator));
    }
    Request& Request::proxy(std::string const& url)
    {
        check(curl_easy_setopt(instance_, CURLOPT_PROXY, url.c_str()));
        return *this;
    }
    Request& Request::tunnel(std::string const& url)
    {
        check(curl_easy_setopt(instance_, CURLOPT_PROXY, url.c_str()));
        check(curl_easy_setopt(instance_, CURLOPT_HTTPPROXYTUNNEL, 1L));
        return *this;
    }
    Request& Request::unixSocket(std::filesystem::path const& path)
    {
        check(curl_easy_setopt(instance_, CURLOPT_UNIX_SOCKET_PATH, path.c_str()));
        return *this;
    }
    Request& Request::method(std::string const& method)
    {
        verb(method);
        return *this;
    }
    Instance& Request::instance()
    {
        return instance_;
    }

    void Request::check(CURLcode code)
    {
        if (code != CURLE_OK)
            throw CurlError("Curl function returned error code.", code);
    }

    Response Request::perform()
    {
        // set header
        struct curl_slist* list = nullptr;
        for (auto const& [key, value] : headerFields_)
        {
            const auto combined = key + ": " + value;
            list = curl_slist_append(list, combined.c_str());
        }
        check(curl_easy_setopt(instance_, CURLOPT_HTTPHEADER, list));
        auto res = Response(instance_);
        for (auto const& sink : sinks_)
        {
            if (sink.second)
                sink.second();
        }
        return res;
    }

    std::ostream& operator<<(std::ostream& stream, Request::LogsType const& logs)
    {
        for (auto const& log : logs)
        {
            switch (log.type)
            {
                case (CURLINFO_TEXT):
                {
                    stream << "* ";
                    break;
                }
                case (CURLINFO_HEADER_IN):
                {
                    stream << "< ";
                    break;
                }
                case (CURLINFO_HEADER_OUT):
                {
                    stream << "> ";
                    break;
                }
                case (CURLINFO_DATA_IN):
                {
                    break;
                }
                case (CURLINFO_DATA_OUT):
                {
                    break;
                }
                case (CURLINFO_SSL_DATA_IN):
                {
                    stream << "<S ";
                    break;
                }
                case (CURLINFO_SSL_DATA_OUT):
                {
                    stream << ">S ";
                    break;
                }
                case (CURLINFO_END):
                {
                    stream << "~ ";
                    break;
                }
                default:
                    break;
            }
            stream << log.data;
        }
        return stream;
    }
}
