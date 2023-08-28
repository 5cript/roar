#pragma once

#include <roar/directory_server/default_listing_style.hpp>
#include <roar/detail/bytes_to_human_readable.hpp>
#include <roar/filesystem/special_paths.hpp>
#include <roar/mime_type.hpp>
#include <roar/filesystem/jail.hpp>
#include <roar/routing/request_listener.hpp>
#include <roar/session/session.hpp>
#include <roar/detail/overloaded.hpp>

#include <boost/beast/http/file_body.hpp>

#include <ctime>
#include <utility>
#include <filesystem>
#include <sstream>
#include <chrono>

namespace Roar::Detail
{
    namespace
    {
        inline static std::string to_string(std::filesystem::file_time_type const& ftime)
        {
#ifdef _MSC_VER
            auto cftime =
                std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    std::chrono::utc_clock::to_sys(std::chrono::file_clock::to_utc(ftime))));
#else
#    ifdef _GLIBCXX_RELEASE
#        if _GLIBCXX_RELEASE >= 9
            auto cftime =
                std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    std::chrono::file_clock::to_sys(ftime)));
#        else
            auto cftime =
                std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    std::chrono::__file_clock::to_sys(ftime)));
#        endif
#    endif
#endif
            std::string result(1024, '\0');
#ifdef _MSC_VER
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
            auto size = std::strftime(result.data(), result.size(), "%a %b %e %H:%M:%S %Y", std::localtime(&cftime));
#    pragma clang diagnostic pop
#else
            auto size = std::strftime(result.data(), result.size(), "%a %b %e %H:%M:%S %Y", std::localtime(&cftime));
#endif
            result.resize(size);
            return result;
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
            , onError_{unwrapFlexibleProvider<RequestListenerT, std::function<void(std::string const&)>>(
                  *this->listener_,
                  this->serveInfo_.serveOptions.onError)}
            , onFileServeComplete_{unwrapFlexibleProvider<RequestListenerT, std::function<void(bool)>>(
                  *this->listener_,
                  this->serveInfo_.serveOptions.onFileServeComplete)}
        {
            if (!onError_)
                onError_ = [](std::string const&) {};
            if (!onFileServeComplete_)
                onFileServeComplete_ = [](bool) {};
        }

        void operator()(Session& session, EmptyBodyRequest const& req)
        {
            namespace http = boost::beast::http;

            // Do now allow unsecure connections if not explicitly allowed
            if (this->serverIsSecure_ && !session.isSecure() && !this->serveInfo_.routeOptions.allowUnsecure)
                return session.sendStrictTransportSecurityResponse();

            const auto fileAndStatus = getFileAndStatus(req.path());

            // File is found, now ask the library user for permissions:
            switch (std::invoke(
                this->serveInfo_.handler, *this->listener_, session, req, fileAndStatus, this->serveInfo_.serveOptions))
            {
                case (ServeDecision::Continue):
                    break;
                case (ServeDecision::Deny):
                    return session.sendStandardResponse(boost::beast::http::status::forbidden);
                case (ServeDecision::Handled):
                    return;
            };

            // Filter allowed methods
            switch (req.method())
            {
                case (http::verb::head):
                {
                    if (!unwrapFlexibleProvider<RequestListenerT, bool>(
                            *this->listener_, this->serveInfo_.serveOptions.allowDownload))
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    return sendHeadResponse(session, req, fileAndStatus);
                }
                case (http::verb::options):
                {
                    return sendOptionsResponse(session, req, fileAndStatus);
                }
                case (http::verb::get):
                {
                    if (!unwrapFlexibleProvider<RequestListenerT, bool>(
                            *this->listener_, this->serveInfo_.serveOptions.allowDownload))
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    break;
                }
                case (http::verb::delete_):
                {
                    if (!unwrapFlexibleProvider<RequestListenerT, bool>(
                            *this->listener_, this->serveInfo_.serveOptions.allowDelete))
                        return session.sendStandardResponse(http::status::method_not_allowed);
                    break;
                }
                case (http::verb::put):
                {
                    if (!unwrapFlexibleProvider<RequestListenerT, bool>(
                            *this->listener_, this->serveInfo_.serveOptions.allowUpload))
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

            handleFileServe(session, req, fileAndStatus);
        }

      private:
        std::filesystem::path resolvePath() const
        {
            const auto rawPath = unwrapFlexibleProvider<RequestListenerT, std::filesystem::path>(
                *this->listener_, this->serveInfo_.serveOptions.pathProvider);
            return Roar::resolvePath(rawPath);
        }

        std::string listingStyle() const
        {
            auto style = unwrapFlexibleProvider<RequestListenerT, std::string>(
                *this->listener_, this->serveInfo_.serveOptions.customListingStyle);
            if (!style)
                style = Detail::defaultListingStyle;
            return *style;
        }

        FileAndStatus getFileAndStatus(boost::beast::string_view target) const
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
            if (target.size() == basePath_.size())
                return analyzeFile("");
            target.remove_prefix(basePath_.size() + (basePath_.size() == 1 ? 0 : 1));
            return analyzeFile(target);
        }

        void handleFileServe(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus) const
        {
            namespace http = boost::beast::http;
            switch (req.method())
            {
                case (http::verb::get):
                {
                    if (fileAndStatus.status.type() == std::filesystem::file_type::directory)
                    {
                        if (unwrapFlexibleProvider<RequestListenerT, bool>(
                                *this->listener_, this->serveInfo_.serveOptions.allowListing))
                            return makeListing(session, req, fileAndStatus);
                        else
                        {
                            return download(
                                session,
                                req,
                                getFileAndStatus((basePath_ / fileAndStatus.relative / "index.html").string()));
                        }
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
                        if (unwrapFlexibleProvider<RequestListenerT, bool>(
                                *this->listener_, this->serveInfo_.serveOptions.allowDeleteOfNonEmptyDirectories))
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

        void sendHeadResponse(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus) const
        {
            namespace http = boost::beast::http;
            const auto contentType = extensionToMime(fileAndStatus.file.extension().string());

            if (fileAndStatus.status.type() != std::filesystem::file_type::regular &&
                fileAndStatus.status.type() != std::filesystem::file_type::symlink)
                return session.sendStandardResponse(http::status::not_found);

            session.send<http::empty_body>(req)
                ->status(http::status::ok)
                .setHeader(http::field::accept_ranges, "bytes")
                .setHeader(http::field::content_length, std::to_string(std::filesystem::file_size(fileAndStatus.file)))
                .contentType(contentType ? *contentType : "application/octet-stream")
                .commit()
                .fail([onError = onError_](auto&& err) {
                    onError(err.toString());
                });
        }

        void sendOptionsResponse(Session& session, EmptyBodyRequest const& req, FileAndStatus const&) const
        {
            namespace http = boost::beast::http;
            std::string allow = "OPTIONS";
            if (unwrapFlexibleProvider<RequestListenerT, bool>(
                    *this->listener_, this->serveInfo_.serveOptions.allowDownload))
                allow += ", GET, HEAD";
            if (unwrapFlexibleProvider<RequestListenerT, bool>(
                    *this->listener_, this->serveInfo_.serveOptions.allowUpload))
                allow += ", PUT";
            if (unwrapFlexibleProvider<RequestListenerT, bool>(
                    *this->listener_, this->serveInfo_.serveOptions.allowDelete))
                allow += ", DELETE";

            session.send<http::empty_body>(req)
                ->status(http::status::no_content)
                .setHeader(http::field::allow, allow)
                .setHeader(http::field::accept_ranges, "bytes")
                .commit();
        }

        void makeListing(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus) const
        {
            namespace http = boost::beast::http;
            std::stringstream document;
            document << "<!DOCTYPE html>\n";
            document << "<html>\n";
            document << "<head>\n";
            document << "<meta charset='utf-8'>\n";
            document << "<meta http-equiv='X-UA-Compatible' content='IE=edge'>\n";
            document << "<style>\n";
            document << listingStyle() << "</style>\n";
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

            try
            {
                for (auto const& entry : std::filesystem::directory_iterator{fileAndStatus.file})
                {
                    std::string link;
                    std::string fn = entry.path().filename().string();
                    auto relative = fileAndStatus.relative.string();
                    link.reserve(relative.size() + basePath_.size() + fn.size() + 10);
                    if (basePath_.empty() || basePath_.front() != '/')
                        link.push_back('/');
                    link += basePath_;
                    if (!basePath_.empty() && basePath_.back() != '/')
                        link.push_back('/');

                    if (!relative.empty() && relative != ".")
                    {
                        link += relative;
                        link.push_back('/');
                    }
                    link += fn;

                    document << "<tr>\n";
                    document << "<td>\n";
                    document << "<a href='" << link << "'>" << fn << "</a>\n";
                    if (entry.is_regular_file())
                    {
                        document << "<td>" << to_string(entry.last_write_time()) << "</td>\n";
                        document << "<td>" << bytesToHumanReadable<1024>(entry.file_size()) << "</td>\n";
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

        void upload(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus) const
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
                    if (unwrapFlexibleProvider<RequestListenerT, bool>(
                            *this->listener_, this->serveInfo_.serveOptions.allowOverwrite))
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

            auto body = std::make_shared<http::file_body::value_type>();
            boost::beast::error_code ec;
            body->open(fileAndStatus.file.string().c_str(), boost::beast::file_mode::write, ec);
            if (ec)
                return session.sendStandardResponse(
                    http::status::internal_server_error, "Cannot open file for writing.");

            session.send<http::empty_body>(req)
                ->status(http::status::continue_)
                .commit()
                .then([session = session.shared_from_this(),
                       req,
                       body = std::move(body),
                       contentLength,
                       onError = onError_](bool closed) {
                    if (closed)
                        return;

                    session->read<http::file_body>(req, std::move(*body))
                        ->bodyLimit(*contentLength)
                        .commit()
                        .then([](auto& session, auto const&) {
                            session.sendStandardResponse(http::status::ok);
                        })
                        .fail([session, onError](Error const& e) {
                            onError(e.toString());
                            session->sendStandardResponse(http::status::internal_server_error, e.toString());
                        });
                });
        }

        void download(Session& session, EmptyBodyRequest const& req, FileAndStatus const& fileAndStatus) const
        {
            namespace http = boost::beast::http;
            if (fileAndStatus.status.type() != std::filesystem::file_type::regular &&
                fileAndStatus.status.type() != std::filesystem::file_type::symlink)
                return session.sendStandardResponse(http::status::not_found);

            const auto ranges = req.ranges();
            if (!ranges)
            {
                http::file_body::value_type body;
                boost::beast::error_code ec;
                body.open(fileAndStatus.file.string().c_str(), boost::beast::file_mode::read, ec);
                if (ec)
                    return session.sendStandardResponse(
                        http::status::internal_server_error, "Cannot open file for reading.");

                auto contentType = extensionToMime(fileAndStatus.file.extension().string());
                auto intermediate = session.send<http::file_body>(req, std::move(body));
                intermediate->preparePayload();
                intermediate->enableCors(req, this->serveInfo_.routeOptions.cors);
                intermediate->contentType(contentType ? contentType.value() : "application/octet-stream");
                intermediate->commit()
                    .then([session = session.shared_from_this(), req, onFileServeComplete = onFileServeComplete_](
                              bool wasClosed) {
                        onFileServeComplete(wasClosed);
                    })
                    .fail([onError = onError_](auto&& err) {
                        onError(err.toString());
                    });
            }
            else
            {
                RangeFileBody::value_type body;
                boost::beast::error_code ec;
                body.open(fileAndStatus.file.string().c_str(), std::ios_base::in, ec);
                if (ec)
                    return session.sendStandardResponse(
                        http::status::internal_server_error, "Cannot open file for reading.");
                try
                {
                    body.setReadRanges(*ranges, "plain/text");

                    session.send<RangeFileBody>(req, std::move(body))
                        ->useFixedTimeout(std::chrono::seconds{10})
                        .commit()
                        .then([session = session.shared_from_this(), req, onFileServeComplete = onFileServeComplete_](
                                  bool wasClosed) {
                            onFileServeComplete(wasClosed);
                        })
                        .fail([onError = onError_](auto&& err) {
                            onError(err.toString());
                        });
                }
                catch (...)
                {
                    return session.sendStandardResponse(http::status::bad_request, "Invalid ranges.");
                }
            }
        }

      private:
        Jail jail_;
        std::string basePath_;
        std::function<void(std::string const&)> onError_;
        std::function<void(bool)> onFileServeComplete_;
    };
}