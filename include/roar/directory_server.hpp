#pragma once

#include <roar/filesystem/special_paths.hpp>
#include <roar/mime_type.hpp>
#include <roar/filesystem/jail.hpp>
#include <roar/routing/request_listener.hpp>
#include <roar/session/session.hpp>
#include <roar/detail/overloaded.hpp>

#include <boost/beast/http/file_body.hpp>

#include <utility>
#include <filesystem>
#include <sstream>
#include <chrono>

namespace Roar::Detail
{
    namespace
    {
        static std::string to_string(std::filesystem::file_time_type const& ftime)
        {
            auto tp = std::chrono::file_clock::to_sys(ftime);
            auto cftime =
                std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    std::chrono::file_clock::to_sys(ftime)));
            std::string str = std::asctime(std::localtime(&cftime));
            str.pop_back(); // rm the trailing '\n' put by `asctime`
            return str;
        }
    }

    template <typename RequestListenerT>
    struct DirectoryServerConstructionArgs
    {
        bool serverIsSecure_;
        ServeInfo<RequestListenerT> serveInfo_;
        std::shared_ptr<RequestListenerT> listener_;
    };

    /**
     * @brief Internal helper class to serve directories.
     *
     * @tparam RequestListenerT
     */
    template <typename RequestListenerT>
    class DirectoryServer : private DirectoryServerConstructionArgs<RequestListenerT>
    {
      public:
        DirectoryServer(DirectoryServerConstructionArgs<RequestListenerT>&& args)
            : DirectoryServerConstructionArgs<RequestListenerT>{std::move(args)}
            , jail_{resolvePath()}
            , basePath_{this->serveInfo_.path}
        {}

        void operator()(Session& session, EmptyBodyRequest const& req)
        {
            namespace http = boost::beast::http;

            // Do now allow unsecure connections if not explicitly allowed
            if (this->serverIsSecure_ && !session.isSecure() && !this->serveInfo_.routeOptions.allowUnsecure)
                return session.sendStrictTransportSecurityResponse();

            const auto fileAndStatus = getFileAndStatus(req.target());

            // Filter allowed methods
            switch (req.method())
            {
                case (http::verb::head):
                {
                    if (!this->serveInfo_.serveOptions.allowDownload)
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    return sendHeadResponse(session, req, fileAndStatus);
                }
                case (http::verb::options):
                {
                    return sendOptionsResponse(session, req, fileAndStatus);
                }
                case (http::verb::get):
                {
                    if (!this->serveInfo_.serveOptions.allowDownload)
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    break;
                }
                case (http::verb::delete_):
                {
                    if (!this->serveInfo_.serveOptions.allowDelete)
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    break;
                }
                case (http::verb::put):
                {
                    if (!this->serveInfo_.serveOptions.allowUpload)
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    break;
                }
                default:
                {
                    return session.sendStandardResponse(http::status::method_not_allowed);
                }
            }

            // Handle not found files and directories:
            if (fileAndStatus.status.type() == std::filesystem::file_type::none && req.method() != http::verb::put)
                return session.sendStandardResponse(http::status::not_found);

            // File is found, method is allowed, now ask the library user for final permissions:
            switch (std::invoke(this->serveInfo_.handler, *this->listener_, session, req, fileAndStatus))
            {
                case (ServeDecision::Continue):
                    return handleFileServe(session, req, fileAndStatus);
                case (ServeDecision::Deny):
                    return session.sendStandardResponse(boost::beast::http::status::forbidden);
                case (ServeDecision::JustClose):
                    return;
            };
        }

      private:
        std::filesystem::path resolvePath()
        {
            const auto rawPath = std::visit(
                Detail::overloaded{
                    [this](std::function<std::filesystem::path(RequestListenerT & requestListener)> const& fn) {
                        return fn(*this->listener_);
                    },
                    [this](std::filesystem::path (RequestListenerT::*fn)()) {
                        return (this->listener_.get()->*fn)();
                    },
                    [this](std::filesystem::path (RequestListenerT::*fn)() const) {
                        return (this->listener_.get()->*fn)();
                    },
                    [this](std::filesystem::path RequestListenerT::*mem) {
                        return this->listener_.get()->*mem;
                    },
                },
                this->serveInfo_.serveOptions.pathProvider);

            return Roar::resolvePath(rawPath);
        }

        FileAndStatus getFileAndStatus(boost::beast::string_view target)
        {
            auto analyzeFile = [this](boost::beast::string_view target) -> FileAndStatus {
                const auto absolute = jail_.pathAsIsInJail(std::filesystem::path{std::string{target}});
                if (absolute)
                {
                    const auto relative = jail_.relativeToRoot(std::filesystem::path{std::string{target}});
                    if (std::filesystem::exists(*absolute))
                        return {.file = *absolute, .relative = *relative, .status = std::filesystem::status(*absolute)};
                    else
                        return {.file = *absolute, .relative = *relative, .status = std::filesystem::file_status{}};
                }
                return {.file = {}, .status = std::filesystem::file_status{}};
            };

            std::pair<std::filesystem::file_type, std::filesystem::path> fileAndStatus;
            if (target.size() <= basePath_.size() + 1)
            {
                if (this->serveInfo_.serveOptions.allowListing)
                    return analyzeFile("");
                else
                    return analyzeFile("index.html");
            }
            target.remove_prefix(basePath_.size() + 1);
            return analyzeFile(target);
        }

        void handleFileServe(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus)
        {
            namespace http = boost::beast::http;
            switch (req.method())
            {
                case (http::verb::get):
                {
                    if (fileAndStatus.status.type() == std::filesystem::file_type::directory)
                    {
                        if (this->serveInfo_.serveOptions.allowListing)
                            return makeListing(session, req, fileAndStatus);
                        else
                            return session.sendStandardResponse(http::status::forbidden);
                    }
                    else
                        return download(session, req, fileAndStatus);
                }
                case (http::verb::delete_):
                {
                    std::error_code ec;
                    auto type = fileAndStatus.status.type();
                    if (type == std::filesystem::file_type::regular || type == std::filesystem::file_type::symlink)
                    {
                        std::filesystem::remove(fileAndStatus.file, ec);
                    }
                    else if (fileAndStatus.status.type() == std::filesystem::file_type::directory)
                    {
                        if (this->serveInfo_.serveOptions.allowDeleteOfNonEmptyDirectories)
                            std::filesystem::remove_all(fileAndStatus.file, ec);
                        else
                        {
                            if (std::filesystem::is_empty(fileAndStatus.file))
                                std::filesystem::remove(fileAndStatus.file, ec);
                            else
                                return session.sendStandardResponse(http::status::forbidden);
                        }
                    }
                    else
                    {
                        ec = std::make_error_code(std::errc::operation_not_supported);
                    }

                    if (ec)
                        return session.sendStandardResponse(http::status::bad_request, ec.message());
                    else
                        return (void)session.send<http::empty_body>(req)->status(http::status::no_content).commit();
                }
                case (http::verb::put):
                {
                    return upload(session, req, fileAndStatus);
                }
                // Cannot happen:
                default:
                    return;
            }
        }

        void sendHeadResponse(Session& session, EmptyBodyRequest const& req, FileAndStatus const&)
        {
            namespace http = boost::beast::http;
            session.send<http::empty_body>(req)
                ->status(http::status::ok)
                // TODO:
                //.setHeader(http::field::accept_ranges, "bytes")
                .commit();
        }

        void sendOptionsResponse(Session& session, EmptyBodyRequest const& req, FileAndStatus const&)
        {
            // TODO: cors

            namespace http = boost::beast::http;
            std::string allow = "OPTIONS";
            if (this->serveInfo_.serveOptions.allowDownload)
                allow += ", GET, HEAD";
            if (this->serveInfo_.serveOptions.allowUpload)
                allow += ", PUT";
            if (this->serveInfo_.serveOptions.allowDelete)
                allow += ", DELETE";

            session.send<http::empty_body>(req)
                ->status(http::status::no_content)
                .setHeader(http::field::allow, allow)
                .commit();
        }

        void makeListing(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus)
        {
            namespace http = boost::beast::http;
            std::stringstream document;
            document << "<!DOCTYPE html>\n";
            document << "<html>\n";
            document << "<head>\n";
            document << "<meta charset='utf-8'>\n";
            document << "<meta http-equiv='X-UA-Compatible' content='IE=edge'>\n";
            document << "<style>\n";
            document << "a { text-decoration: none; }\n";
            document << "a:hover { text-decoration: underline; }\n";
            document << "a:visited { color: #23f5fc; }\n";
            document << "a:active { color: #23fc7e; }\n";
            document << "a:link { color: #83a8f2; }\n";
            document << R"css(
body { 
    font-family: sans-serif; 
    font-size: 0.8em;
    width: 100%;
    margin: 0;
    padding: 8px;
    background-color: #303030;
    color: #eee;
}

.styled-table {
    border-collapse: collapse;
    margin: 25px 0;
    min-width: 400px;
    width: calc(100% - 16px);
    box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);
}

.styled-table thead tr {
    background-color: #0a5778;
    color: #ffffff;
    text-align: left;
}

.styled-table th,
.styled-table td {
    padding: 12px 15px;
}

.styled-table tbody tr {
    border-bottom: 1px solid #dddddd;
}

.styled-table tbody tr:nth-of-type(even) {
    background-color: #505050;
}

.styled-table tbody tr:last-of-type {
    border-bottom: 2px solid #0a5778;
}

.styled-table tbody tr.active-row {
    font-weight: bold;
    color: #009879;
}
            )css";
            document << "</style>\n";
            document << "<title>Roar Listing of " << fileAndStatus.relative.string() << "</title>\n";
            document << "<meta name='viewport' content='width=device-width, initial-scale=1'>\n";
            document << "</head>\n";
            document << "<body>\n";
            document << "<h1>Index of " << fileAndStatus.relative.string() << "</h1>\n";
            document << "<table class=\"styled-table\">\n";
            document << "<thead>\n";
            document << "<tr>\n";
            document << "<th>"
                     << "Name"
                     << "</th>\n";
            document << "<th>"
                     << "Last Modified"
                     << "</th>\n";
            document << "<th>"
                     << "Size"
                     << "</th>\n";
            document << "</tr>\n";
            document << "</thead>\n";
            document << "<tbody>\n";

            auto toHumanReadableSize = [](auto size) {
                if (size < 1024)
                    return std::to_string(size) + " bytes";
                else if (size < 1024 * 1024)
                    return std::to_string(size / 1024) + " KB";
                else if (size < 1024 * 1024 * 1024)
                    return std::to_string(size / 1024 / 1024) + " MB";
                else
                    return std::to_string(size / 1024 / 1024 / 1024) + " GB";
            };

            try
            {
                for (auto const& entry : std::filesystem::directory_iterator{fileAndStatus.file})
                {
                    document << "<tr>\n";
                    document << "<td>\n";
                    document << "<a href='" << basePath_ << "/" << fileAndStatus.relative.string() << "/"
                             << entry.path().filename().string() << "'>" << entry.path().filename().string()
                             << "</a>\n";
                    if (entry.is_regular_file())
                    {
                        document << "<td>" << to_string(entry.last_write_time()) << "</td>\n";
                        document << "<td>" << toHumanReadableSize(entry.file_size()) << "</td>\n";
                    }
                    else
                    {
                        document << "<td>"
                                 << "</td>\n";
                        document << "<td>"
                                 << "</td>\n";
                    }
                    document << "</td>\n";
                    document << "</tr>\n";
                }
            }
            catch (std::exception const& exc)
            {
                return session.sendStandardResponse(boost::beast::http::status::internal_server_error, exc.what());
            }

            document << "</table>\n";
            document << "</tbody>\n";
            document << "</body>\n";
            document << "</html>\n";

            session.send<http::string_body>(req)
                ->status(http::status::ok)
                .contentType("text/html")
                .body(document.str())
                .commit();
        }

        void upload(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus)
        {
            namespace http = boost::beast::http;
            if (!req.expectsContinue())
            {
                return (void)session.send<http::string_body>(req)
                    ->status(http::status::expectation_failed)
                    .setHeader(http::field::connection, "close")
                    .body("Set Expect: 100-continue")
                    .commit();
            }

            switch (fileAndStatus.status.type())
            {
                case (std::filesystem::file_type::none):
                    break;
                case (std::filesystem::file_type::not_found):
                    break;
                case (std::filesystem::file_type::regular):
                {
                    if (this->serveInfo_.serveOptions.allowOverwrite)
                        break;
                }
                default:
                {
                    return session.sendStandardResponse(http::status::forbidden);
                }
            }

            auto contentLength = req.contentLength();
            if (!contentLength)
                return session.sendStandardResponse(http::status::bad_request, "Require Content-Length.");

            // TODO: Range Requests
            auto body = std::make_shared<http::file_body::value_type>();
            boost::beast::error_code ec;
            body->open(fileAndStatus.file.string().c_str(), boost::beast::file_mode::write, ec);
            if (ec)
                return session.sendStandardResponse(
                    http::status::internal_server_error, "Cannot open file for writing.");

            session.send<http::empty_body>(req)
                ->status(http::status::continue_)
                .commit()
                .then([session = session.shared_from_this(), req, body = std::move(body), contentLength](bool closed) {
                    if (closed)
                        return;

                    session->read<http::file_body>(req, std::move(*body))
                        ->bodyLimit(*contentLength)
                        .commit()
                        .then([](auto& session, auto const& req) {
                            session.sendStandardResponse(http::status::ok);
                        })
                        .fail([session](Error const& e) {
                            session->sendStandardResponse(http::status::internal_server_error, e.toString());
                        });
                });
        }

        void download(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus)
        {
            namespace http = boost::beast::http;
            // TODO: Range Requests
            http::file_body::value_type body;
            boost::beast::error_code ec;
            body.open(fileAndStatus.file.string().c_str(), boost::beast::file_mode::read, ec);
            if (ec)
                return session.sendStandardResponse(
                    http::status::internal_server_error, "Cannot open file for reading.");

            auto intermediate = session.send<http::file_body>(req, std::move(body));
            intermediate->preparePayload();
            intermediate->enableCors(req, this->serveInfo_.routeOptions.cors);
            auto contentType = extensionToMime(fileAndStatus.file.extension().string());
            if (contentType)
                intermediate->contentType(*contentType);
            intermediate->commit();
        }

      private:
        Jail jail_;
        std::string basePath_;
    };
}