#pragma once

#include "instance.hpp"
#include "response.hpp"
#include "source.hpp"

#include <boost/beast/http/field.hpp>
#include <curl/curl.h>
#ifdef ROAR_ENABLE_NLOHMANN_JSON
#    include <nlohmann/json.hpp>
#endif

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Roar::Curl
{
    class Sink;

    struct VerboseLogEntry
    {
        curl_infotype type;
        std::string data;
    };

    class Request
    {
      public:
        using LogsType = std::vector<VerboseLogEntry>;

        Request();

        /**
         * @brief Sets a basic auth header.
         *
         * @param name User name
         * @param password User password.
         * @return Request& Returned for chaining.
         */
        Request& basicAuth(std::string const& name, std::string const& password);

        /**
         * @brief Sets a bearer auth header.
         *
         * @param bearerToken The token to send.
         * @return Request& Returned for chaining.
         */
        Request& bearerAuth(std::string const& bearerToken);

        /**
         * @brief Sets the Accept-Encoding haeder.
         *
         * @param encoding The accepted encoding.
         * @return Request& Returned for chaining.
         */
        Request& acceptEncoding(std::string const& encoding);

        /**
         * @brief Set the all header fields at one using the map.
         *
         * @param fields A key/value object for header fields
         * @return Request& Returned for chaining.
         */
        Request& setHeaderFields(std::unordered_map<std::string, std::string> const& fields);

        /**
         * @brief Set the speficied header field with value
         *
         * @param key The field.
         * @param value The value.
         * @return Request& Returned for chaining.
         */
        Request& setHeader(std::string const& key, std::string const& value);

        /**
         * @brief Set the speficied header field with value
         *
         * @param key The field.
         * @param value The value.
         * @return Request& Returned for chaining.
         */
        Request& setHeader(boost::beast::http::field key, std::string const& value);

        /**
         * @brief Verify the peer in secure requests?
         *
         * @param verify verify yes/no?
         * @return Request& Returned for chaining.
         */
        Request& verifyPeer(bool verify = true);

        /**
         * @brief Verify host in secure request?
         *
         * @param verify verify yes/no?
         * @return Request& Returned for chaining.
         */
        Request& verifyHost(bool verify = true);

        /**
         * @brief Set a timeout for connects.
         *
         * @param timeout The timeout in milliseconds.
         * @return Request& Returned for chaining.
         */
        Request& connectTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Set a timeout for transfers.
         *
         * @param timeout The timeout in milliseconds.
         * @return Request& Returned for chaining.
         */
        Request& transferTimeout(std::chrono::milliseconds timeout);

        /**
         * @brief Shall curl use signals?
         *
         * @param useSignal use signals yes/no?
         * @return Request& Returned for chaining.
         */
        Request& useSignals(bool useSignal);

        /**
         * @brief A sink that is a simple function accepting the data.
         *
         * @param sinkFunction This function will be repeatedly called with new data.
         * @return Request& Returned for chaining
         */
        Request& sink(
            std::function<void(char const*, std::size_t)> const& sinkFunction,
            std::function<void()> onComplete = []() {})
        {
            sinks_.emplace_back(
                std::pair<std::function<void(char*, std::size_t)>, std::function<void()>>(sinkFunction, onComplete));
            return *this;
        }

#ifdef ROAR_ENABLE_NLOHMANN_JSON
        Request& sink(nlohmann::json& json);
#endif

        /**
         * @brief Installs a sink to a string.
         *
         * @param str The string that should be written to.
         * @return Request& Returned for chaining.
         */
        Request& sink(std::string& str);

        /**
         * @brief Installs a sink to a file.
         *
         * This function does not check whether the path is writeable to.
         * @param path The path that should be written to.
         * @return Request& Returned for chaining.
         */
        Request& sink(std::filesystem::path const& path);

        /**
         * @brief Installs a custom sink
         *
         * @param customSink A sink deriving from Roar::Curl::Sink
         * @return Request& Returned for chaining.
         */
        Request& sink(Sink& customSink);

        /**
         * @brief Reenables outputting to stdout.
         *
         * @return Request& Returned for chaining.
         */
        Request& addLibcurlDefaultSink();

        /**
         * @brief Enables the use of a proxy.
         *
         * @param url The url to the proxy.
         * @return Request& Returned for chaining.
         */
        Request& proxy(std::string const& url);

        /**
         * @brief Enables the use of a proxy tunnel (HTTPS proxy).
         *
         * @param url The url to the proxy.
         * @return Request& Returned for chaining.
         */
        Request& tunnel(std::string const& url);

        /**
         * @brief Use unix socket instead of a tcp socket.
         *
         * @param path Path to the unix socket.
         * @return Request& Returned for chaining.
         */
        Request& unixSocket(std::filesystem::path const& path);

        /**
         * @brief Sets the Content-Type header.
         *
         * @param type The content type to set.
         * @return Request& Returned for chaining.
         */
        Request& contentType(std::string const& type);

        /**
         * @brief Enables / Disables verbose logging.
         *
         * @param enable enable verbose logging yes/no
         * @return Request& Returned for chaining.
         */
        Request& verbose(bool enable = true);

        /**
         * @brief Will enable verbose logging and log to log container
         *
         * @param logs The log container to log to. You must keep it alive.
         * @return Request& Returned for chaining.
         */
        Request& verbose(LogsType& logs);

        /**
         * @brief The passed function is called when a header is received.
         *
         * @param headerReceiver A function taking header values.
         * @return Request& Returned for chaining.
         */
        Request& onHeaderValue(std::function<void(std::string const&, std::string const&)> const& headerReceiver);

        /**
         * @brief Register a sink for header values, that takes all received headers.
         *
         * @param headers A container that takes the header values. You must keep it alive.
         * @return Request& Returned for chaining.
         */
        Request& headerSink(std::unordered_map<std::string, std::string>& headers);

        /**
         * @brief Inplace construct a source that is used for the sent body.
         *
         * @tparam SourceT The source type (cannot be inferred).
         * @tparam Args Forwarded args to the source constructor.
         * @param args The forwarded args.
         * @return Request& Returned for chaining.
         */
        template <typename SourceT, typename... Args>
        Request& emplaceSource(Args&&... args)
        {
            source_ = std::make_unique<SourceT>(std::forward<Args>(args)...);
            setupSource(source_->isChunked());
            return *this;
        }

        /**
         * @brief Set the source from a c-string.
         *
         * @param str A string source.
         * @return Request& Returned for chaining.
         */
        Request& source(char const* str);

        /**
         * @brief Set the request method
         *
         * @param method The method to set.
         * @return Request& Returned for chaining.
         */
        Request& method(std::string const& method);

        /**
         * @brief Alias for method.
         *
         * @param verb
         */
        Request& verb(std::string const& verb);

        /**
         * @brief Set the source from a string.
         *
         * @param str A string source.
         * @return Request& Returned for chaining.
         */
        Request& source(std::string str);

        /**
         * @brief Set the source to be a file.
         *
         * @param str A file source
         * @return Request& Returned for chaining.
         */
        Request& source(std::filesystem::path path);

        /**
         * @brief Finishes and performs the request as a get request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response get(std::string const& url);

        /**
         * @brief Finishes and performs the request as a put request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response put(std::string const& url);

        /**
         * @brief Finishes and performs the request as a post request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response post(std::string const& url);

        /**
         * @brief Finishes and performs the request as a delete request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response delete_(std::string const& url);

        /**
         * @brief Finishes and performs the request as a options request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response options(std::string const& url);

        /**
         * @brief Finishes and performs the request as a head request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response head(std::string const& url);

        /**
         * @brief Finishes and performs the request as a patch request.
         *
         * @param url The target to send this request to.
         * @return Request& Returned for chaining.
         */
        Response patch(std::string const& url);

        /**
         * @brief Returns the underlying curl instance.
         */
        Instance& instance();

        /**
         * @brief The the url of the request.
         *
         * @param url
         */
        Request& url(std::string const& url);

        /**
         * @brief Perform the request. Use this like verb("GET").url("bla.com").perform().
         * There are get,patch,put etc which combine these into one.
         */
        Response perform();

      private:
        void check(CURLcode code);
        void setupSource(bool chunked);
        void setupSinks();

      private:
        Instance instance_;
        std::function<void(char*, std::size_t)> sink_;
        std::vector<std::pair<std::function<void(char*, std::size_t)>, std::function<void()>>> sinks_;
        std::unique_ptr<Source> source_;
        std::vector<std::function<void(std::string const&, std::string const&)>> headerReceivers_;
        std::unordered_map<std::string, std::string> headerFields_;
    };

    std::ostream& operator<<(std::ostream& stream, Request::LogsType const& logs);
}
