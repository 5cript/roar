#pragma once

#include <boost/leaf.hpp>

#include <memory>

namespace boost::asio
{
    class any_io_executor;

    namespace ip
    {
        class tcp;
        template <typename T>
        class basic_endpoint;
    }
    namespace ssl
    {
        class context;
    }
}

namespace Roar
{
    class Server
    {
      public:
        /**
         * @brief Construct a new Server object given a boost asio io_executor.
         *
         * @param executor A boost asio executor.
         */
        Server(boost::asio::any_io_executor& executor);

        /**
         * @brief Construct a new Server object given a boost asio io_executor.
         *
         * @param executor A boost asio executor.
         * @param sslContext A boost asio ssl context for encryption support.
         */
        Server(boost::asio::any_io_executor& executor, boost::asio::ssl::context&& sslContext);

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
        boost::leaf::result<void> start(std::string const& host = "::", unsigned short port = 443);

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

      private:
        struct Implementation;
        std::shared_ptr<Implementation> impl_;
    };
}