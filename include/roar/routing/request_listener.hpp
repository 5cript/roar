#pragma once

/** @file */

#include <roar/routing/proto_route.hpp>
#include <roar/utility/overloaded.hpp>
#include <roar/literals/regex.hpp>
#include <roar/session/session.hpp>
#include <roar/routing/flexible_provider.hpp>
#include <roar/request.hpp>

#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/describe/class.hpp>

#include <string_view>
#include <optional>
#include <variant>
#include <regex>
#include <string>
#include <filesystem>

namespace Roar
{
    class Server;

    enum class ServeDecision
    {
        /// Continue to serve/delete/upload the file.
        Continue,

        /// Will respond with a standard forbidden response.
        Deny,

        /// You handled the response, so just end the connection if necessary.
        Handled
    };

    template <typename RequestListenerT>
    using HandlerType = void (RequestListenerT::*)(Session&, Request<boost::beast::http::empty_body>&&);

    enum class RoutePathType
    {
        Unspecified,

        /// Take precedence over any other path
        RegularString,

        /// Take precedence over serve paths
        Regex,

        /// Taken into account last.
        ServedDirectory
    };

    template <typename RequestListenerT>
    struct ServeOptions
    {
        /// Allow GET requests to download files.
        FlexibleProvider<RequestListenerT, bool> allowDownload = true;

        /// Allow PUT requests to upload files.
        FlexibleProvider<RequestListenerT, bool> allowUpload = false;

        /// Allow PUT requests to overwrite existing files.
        FlexibleProvider<RequestListenerT, bool> allowOverwrite = false;

        /// Allow DELETE requests to delete files.
        FlexibleProvider<RequestListenerT, bool> allowDelete = false;

        /// Allow DELETE for directories that are non empty?
        FlexibleProvider<RequestListenerT, bool> allowDeleteOfNonEmptyDirectories = false;

        /// Requests on directories become listings.
        FlexibleProvider<RequestListenerT, bool> allowListing = true;

        /// Serve files from the directory given by this function.
        FlexibleProvider<RequestListenerT, std::filesystem::path> pathProvider = std::filesystem::path{};

        /// Serve files from the directory given by this function.
        FlexibleProvider<RequestListenerT, std::string, true> customListingStyle = std::monostate{};

        /// Called when errors occur on serve.
        FlexibleProvider<RequestListenerT, std::function<void(std::string const& err)>> onError =
            std::function<void(std::string const& err)>{};

        /// Called when errors occur on serve file complete. bool parameter = was the connection closed?
        FlexibleProvider<RequestListenerT, std::function<void(bool)>> onFileServeComplete = std::function<void(bool)>{};
    };

    struct FileAndStatus
    {
        std::filesystem::path file{};
        std::filesystem::path relative{};
        std::filesystem::file_status status{};
    };
    template <typename RequestListenerT>
    using ServeHandlerType = ServeDecision (RequestListenerT::*)(
        Session&,
        Request<boost::beast::http::empty_body> const&,
        FileAndStatus const&,
        ServeOptions<RequestListenerT>& options);

#define ROUTE_INFO_COMMON() \
    char const* path = nullptr; \
    RouteOptions routeOptions = { \
        .allowUnsecure = false, \
        .expectUpgrade = false, \
        .cors = std::nullopt, \
    };

    /**
     * @brief This class is what is used in ROAR_GET, ROAR_PUT, ... requests
     * All options you can set for routes behind these macros are part of this object.
     *
     * @tparam RequestListenerT The request listener class that the route belongs to.
     */
    template <typename RequestListenerT>
    struct RouteInfo
    {
        ROUTE_INFO_COMMON()

        /// What verb for this route?
        std::optional<boost::beast::http::verb> verb = std::nullopt;

        /// Is the path a string or a regex, ...?
        RoutePathType pathType = RoutePathType::RegularString;

        /// Set automatically by the macro.
        HandlerType<RequestListenerT> handler = nullptr;
    };

    template <typename RequestListenerT>
    struct ServeInfo
    {
        ROUTE_INFO_COMMON()

        /// Options needed for serving files.
        ServeOptions<RequestListenerT> serveOptions = {};

        /// Set automatically by the macro.
        ServeHandlerType<RequestListenerT> handler = nullptr;
    };

    template <typename RequestListenerT>
    auto extendRouteInfo(RouteInfo<RequestListenerT> info, HandlerType<RequestListenerT> handler)
    {
        return overloaded{
            [info, handler](RouteInfo<RequestListenerT> userInfo) -> RouteInfo<RequestListenerT> {
                return {
                    .path = userInfo.path,
                    .routeOptions = userInfo.routeOptions,
                    .verb = userInfo.verb ? userInfo.verb : info.verb,
                    .pathType = userInfo.pathType,
                    .handler = handler,
                };
            },
            [info, handler](char const* path) -> RouteInfo<RequestListenerT> {
                return {
                    .path = path,
                    .routeOptions = info.routeOptions,
                    .verb = info.verb,
                    .pathType = info.pathType,
                    .handler = handler,
                };
            },
            [info, handler](PseudoRegex path) -> RouteInfo<RequestListenerT> {
                return {
                    .path = path.pattern,
                    .routeOptions = info.routeOptions,
                    .verb = info.verb,
                    .pathType = RoutePathType::Regex,
                    .handler = handler,
                };
            },
        };
    }

    template <typename RequestListenerT>
    auto extendRouteInfoForServe(ServeInfo<RequestListenerT> info, ServeHandlerType<RequestListenerT> handler)
    {
        return overloaded{
            [info, handler](ServeInfo<RequestListenerT> userInfo) -> ServeInfo<RequestListenerT> {
                return {
                    .path = userInfo.path,
                    .routeOptions = userInfo.routeOptions,
                    .serveOptions = userInfo.serveOptions,
                    .handler = handler,
                };
            },
            [info, handler](char const* path, std::filesystem::path const& root) -> ServeInfo<RequestListenerT> {
                return {
                    .path = path,
                    .routeOptions = info.routeOptions,
                    .serveOptions =
                        {
                            .pathProvider = root,
                        },
                    .handler = handler,
                };
            },
            [info, handler](char const* path, std::filesystem::path (RequestListenerT::*rootProvider)())
                -> ServeInfo<RequestListenerT> {
                return {
                    .path = path,
                    .routeOptions = info.routeOptions,
                    .serveOptions =
                        {
                            .pathProvider = rootProvider,
                        },
                    .handler = handler,
                };
            },
            [info, handler](char const* path, std::filesystem::path (RequestListenerT::*rootProvider)() const)
                -> ServeInfo<RequestListenerT> {
                return {
                    .path = path,
                    .routeOptions = info.routeOptions,
                    .serveOptions =
                        {
                            .pathProvider = rootProvider,
                        },
                    .handler = handler,
                };
            },
            [info, handler](char const* path, std::filesystem::path RequestListenerT::*rootProvider)
                -> ServeInfo<RequestListenerT> {
                return {
                    .path = path,
                    .routeOptions = info.routeOptions,
                    .serveOptions =
                        {
                            .pathProvider = rootProvider,
                        },
                    .handler = handler,
                };
            }};
    }

/// Necessary to make pointer to members.
#define ROAR_MAKE_LISTENER(ListenerType) using this_type = ListenerType

#define ROAR_ROUTE_I(HandlerName, DefaultVerb) \
    void HandlerName(Roar::Session& session, Roar::Request<boost::beast::http::empty_body>&& request); \
    inline static const auto roar_##HandlerName = Roar::extendRouteInfo( \
        Roar::RouteInfo<this_type>{.verb = boost::beast::http::verb::DefaultVerb}, &this_type::HandlerName)

#define ROAR_SERVE(HandlerName) \
    Roar::ServeDecision HandlerName( \
        Roar::Session& session, \
        Roar::Request<boost::beast::http::empty_body> const& request, \
        Roar::FileAndStatus const& fileAndStatus, \
        Roar::ServeOptions<this_type>& options); \
    inline static const auto roar_##HandlerName = \
        Roar::extendRouteInfoForServe(Roar::ServeInfo<this_type>{}, &this_type::HandlerName)

/**
 * @brief Define a route.
 * @ref route
 */
#define ROAR_ROUTE(HandlerName) ROAR_ROUTE_I(HandlerName, get)

/// Define route for GET, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_GET(HandlerName) ROAR_ROUTE_I(HandlerName, get)

/// Define route for POST, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_POST(HandlerName) ROAR_ROUTE_I(HandlerName, post)

/// Define route for PUT, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_PUT(HandlerName) ROAR_ROUTE_I(HandlerName, put)

/// Define route for OPTIONS, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_OPTIONS(HandlerName) ROAR_ROUTE_I(HandlerName, options)

/// Define route for TRACE, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_TRACE(HandlerName) ROAR_ROUTE_I(HandlerName, trace)

/// Define route for HEAD, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_HEAD(HandlerName) ROAR_ROUTE_I(HandlerName, head)

/// Define route for DELETE, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_DELETE(HandlerName) ROAR_ROUTE_I(HandlerName, delete_)

} // namespace Roar