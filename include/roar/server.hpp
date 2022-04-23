#pragma once

#include <__utility/piecewise_construct.h>
#include <boost/describe/modifiers.hpp>
#include <roar/beast/forward.hpp>
#include <roar/routing/proto_route.hpp>
#include <roar/error.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/describe/members.hpp>
#include <boost/leaf.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/beast/http/verb.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <functional>

namespace Roar
{
    class RequestListener;
    class Server
    {
      public:
        struct ConstructionArguments
        {
            /// Required io executor for boost::asio.
            boost::asio::any_io_executor& executor;

            /// Supply for SSL support.
            std::optional<boost::asio::ssl::context> sslContext;

            /// Called when an error occurs in an asynchronous routine.
            std::function<void(Error&&)> onError = [](auto&&) {};

            /// Called when the server stops accepting connections for error reasons.
            std::function<void(boost::system::error_code)> onAcceptAbort = [](auto) {};
        };

        /**
         * @brief Construct a new Server object given a boost asio io_executor.
         *
         * @param constructionArgs Options to construct the server with
         */
        Server(ConstructionArguments constructionArgs);

        ~Server();
        Server(Server const&) = delete;
        Server(Server&&);
        Server& operator=(Server const&) = delete;
        Server& operator=(Server&&);

        /**
         * @brief Bind and listen for network interface and port.
         *
         * @param host An ip / hostname to identify the network interface to listen on.
         * @param port A port to bind on.
         */
        boost::leaf::result<void> start(unsigned short port = 443, std::string const& host = "::");

        /**
         * @brief Starts the server given the already resolved bind endpoint.
         *
         * @param bindEndpoint An endpoint to bind on.
         */
        boost::leaf::result<void> start(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> const& bindEndpoint);

        /**
         * @brief Stop and shutdown the server.
         */
        void stop();

        /**
         * @brief Attach a request listener to this server to receive requests.
         *
         * @param listener
         */
        template <typename RequestListenerT, typename... ConstructionArgsT>
        void installRequestListener(ConstructionArgsT&&... args)
        {
            auto listener = std::make_shared<RequestListenerT>(std::forward<ConstructionArgsT>(args)...);
            // TODO:
            using routes = boost::describe::
                describe_members<RequestListenerT, boost::describe::mod_any_access | boost::describe::mod_static>;
            std::unordered_multimap<boost::beast::http::verb, ProtoRoute> extractedRoutes;
            boost::mp11::mp_for_each<routes>([&extractedRoutes, &listener]<typename T>(T route) {
                ProtoRoute protoRoute{
                    .path = route.pointer->path,
                    .callRoute =
                        [listener]() {
                            // TODO:
                        },
                };
                extractedRoutes.emplace(*route.pointer->verb, protoRoute);
            });
            addRequestListenerToRouter(std::move(extractedRoutes));
        }

      private:
        void addRequestListenerToRouter(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes);

      private:
        struct Implementation;
        std::shared_ptr<Implementation> impl_;
    };
}