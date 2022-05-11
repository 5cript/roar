#pragma once

#include <roar/websocket/read_result.hpp>
#include <roar/error.hpp>
#include <roar/detail/promise_compat.hpp>
#include <roar/detail/stream_type.hpp>

#include <promise-cpp/promise.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <variant>

namespace Roar
{
    class WebsocketBase : public std::enable_shared_from_this<WebsocketBase>
    {
      public:
        WebsocketBase(std::variant<Detail::StreamType, boost::beast::ssl_stream<Detail::StreamType>>&& stream);
        WebsocketBase(std::variant<
                      boost::beast::websocket::stream<boost::beast::ssl_stream<Detail::StreamType>>,
                      boost::beast::websocket::stream<Detail::StreamType>>&& ws);
        WebsocketBase(WebsocketBase&&) = delete;
        WebsocketBase& operator=(WebsocketBase&&) = delete;
        WebsocketBase(WebsocketBase const&) = delete;
        WebsocketBase& operator=(WebsocketBase const&) = delete;
        virtual ~WebsocketBase();

        template <typename FunctionT>
        auto withStreamDo(FunctionT&& func)
        {
            return std::visit(std::forward<FunctionT>(func), ws_);
        }

        template <typename FunctionT>
        auto withStreamDo(FunctionT&& func) const
        {
            return std::visit(std::forward<FunctionT>(func), ws_);
        }

        /**
         * @brief Set the maximum incoming message size option.
         *
         * @param amount
         */
        void readMessageMax(std::size_t amount);

        /**
         * @brief Returns the maximum incoming message size setting.
         *
         * @return std::size_t
         */
        std::size_t readMessageMax() const;

        /**
         * @brief Enable/Disable binary for next send message.
         *
         * @param enable true = binary yes, false = binary no.
         */
        void binary(bool enable);

        /**
         * @brief Returns true if the binary message write option is set.
         *
         * @return true binary is set.
         * @return false binary is not set.
         */
        bool binary() const;

        /**
         * @brief Sends a string to the server.
         *
         * @param message A message to send.
         * @return A promise that is called with the amount of bytes sent, Promise::fail with a
         * Roar::Error.
         */
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<std::size_t>, Detail::PromiseTypeBindFail<Error const&>>
        send(std::string message);

        /**
         * @brief Reads something from the server.
         *
         * @return A promise to continue after reading
         */
        Detail::
            PromiseTypeBind<Detail::PromiseTypeBindThen<WebsocketReadResult>, Detail::PromiseTypeBindFail<Error const&>>
            read();

        /**
         * @brief Set the automatic fragmentation option.
         *
         * @param enable true = automatic fragmentation, false = no automatic fragmentation.
         */
        void autoFragment(bool enable);

        /**
         * @brief Returns true if the automatic fragmentation option is set.
         *
         * @return true yes is set
         * @return false no is not.
         */
        bool autoFragment() const;

        /**
         * @brief Closes the websocket connection.
         *
         * @return true Returns true when the timeout was not reached.
         * @return false Returns false when the timeout was reached.
         */
        bool close(std::chrono::seconds closeWaitTimeout = std::chrono::seconds{3});

      protected:
        std::variant<
            boost::beast::websocket::stream<boost::beast::ssl_stream<Detail::StreamType>>,
            boost::beast::websocket::stream<Detail::StreamType>>
            ws_;
    };
}