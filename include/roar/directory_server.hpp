#pragma once

#include <roar/detail/filesystem/special_paths.hpp>
#include <roar/mime_type.hpp>
#include <roar/detail/filesystem/jail.hpp>
#include <roar/routing/request_listener.hpp>
#include <roar/session/session.hpp>
#include <roar/detail/overloaded.hpp>

#include <boost/beast/http/file_body.hpp>

#include <utility>
#include <filesystem>

namespace Roar::Detail
{
    template <typename RequestListenerT>
    struct DirectoryServerConstructionArgs
    {
        bool serverIsSecure_;
        ServeInfo<RequestListenerT> serveInfo_;
        std::weak_ptr<RequestListenerT> listener_;
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

            // Listener not allive? Fringe Moment on shutdown? This likely indicates missuse.
            auto listener = this->listener_.lock();
            if (!listener)
                return session.sendStandardResponse(http::status::service_unavailable);

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
            switch (std::invoke(this->serveInfo_.handler, *listener, session, req, fileAndStatus))
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
            auto listener = this->listener_.lock();
            if (!listener)
                return {};

            const auto rawPath = std::visit(
                Detail::overloaded{
                    [&listener](std::function<std::filesystem::path(RequestListenerT & requestListener)> const& fn) {
                        return fn(*listener);
                    },
                    [&listener](std::filesystem::path (RequestListenerT::*fn)()) {
                        return (listener.get()->*fn)();
                    },
                    [&listener](std::filesystem::path (RequestListenerT::*fn)() const) {
                        return (listener.get()->*fn)();
                    },
                    [&listener](std::filesystem::path RequestListenerT::*mem) {
                        return listener.get()->*mem;
                    },
                },
                this->serveInfo_.serveOptions.pathProvider);

            return Detail::resolvePath(rawPath);
        }

        FileAndStatus getFileAndStatus(boost::beast::string_view target)
        {
            auto analyzeFile = [this](boost::beast::string_view target) -> FileAndStatus {
                auto relative = jail_.pathAsIsInJail(std::filesystem::path{std::string{target}});
                if (relative)
                {
                    if (std::filesystem::exists(*relative))
                        return {.file = *relative, .status = std::filesystem::status(*relative)};
                    else
                        return {.file = *relative, .status = std::filesystem::file_status{}};
                }
                return {.file = {}, .status = std::filesystem::file_status{}};
            };

            std::pair<std::filesystem::file_type, std::filesystem::path> fileAndStatus;
            if (target.size() <= basePath_.size() + 1)
                return analyzeFile("index.html");
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
                // TODO: .setHeader(http::field::accept_ranges, "bytes")
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
            // TODO: Implement
            namespace http = boost::beast::http;
            session.send<http::string_body>(req)
                ->status(http::status::ok)
                .contentType("text/plain")
                .body("You will see a dir listing here at some point")
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
        Detail::Jail jail_;
        std::string basePath_;
    };
}