#include <roar/client.hpp>

#include <boost/fusion/include/at_c.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

namespace Roar
{
    using namespace promise;
    namespace Detail
    {
        namespace Parser
        {
            namespace x3 = boost::spirit::x3;
            namespace ascii = boost::spirit::x3::ascii;

            using ascii::char_;
            using ascii::alnum;

            const auto key = x3::rule<struct KeyTag, std::string>{"key"} = +(alnum | char_('-') | char_('_'));
            const auto value = x3::rule<struct ValueTag, std::string>{"value"} = +(char_ - '\n');
            const auto newLine = x3::rule<struct NewLineTag, std::string>{"newLine"} = char_('\n');
            const auto whitespace = x3::rule<struct WhitespaceTag, std::string>{"whitespace"} =
                char_(' ') | char_('\t');

            const auto entry = x3::rule<struct EntryTag, std::pair<std::string, std::string>>{"entry"} =
                -(key[([](auto& ctx) {
                      x3::traits::move_to(x3::_attr(ctx), x3::_val(ctx).first);
                  })] > ':' >>
                  *whitespace) >>
                value[([](auto& ctx) {
                    x3::traits::move_to(x3::_attr(ctx), x3::_val(ctx).second);
                })];

            const auto entries =
                x3::rule<struct EntriesTag, std::vector<std::pair<std::string, std::string>>>{"entries"} =
                    entry % newLine;
        }
    }

    // ##################################################################################################################
    Client::Client(ConstructionArguments&& args)
        : socket_{[&args]() -> decltype(socket_) {
            if (args.sslContext)
                return boost::beast::ssl_stream<boost::beast::tcp_stream>{
                    boost::beast::tcp_stream{args.executor}, *args.sslContext};
            else
                return boost::beast::tcp_stream{args.executor};
        }()}
    {
        if (std::holds_alternative<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_))
        {
            auto& sslSocket = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_);
            sslSocket.set_verify_mode(args.sslVerifyMode);
            if (args.sslVerifyCallback)
                sslSocket.set_verify_callback(std::move(args.sslVerifyCallback));
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    Client::~Client()
    {
        shutdownSync();
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<std::size_t>, Detail::PromiseTypeBindFail<Error>>
    Client::send(std::string message, std::chrono::seconds timeout)
    {
        return newPromise([this, message = std::move(message), timeout](Defer d) mutable {
            withStreamDo([this, &d, &message, timeout](auto& socket) mutable {
                withLowerLayerDo([&](auto& socket) {
                    socket.expires_after(timeout);
                });
                std::shared_ptr<std::string> messagePtr = std::make_shared<std::string>(std::move(message));
                boost::asio::async_write(
                    socket,
                    boost::asio::buffer(*messagePtr),
                    [d = std::move(d), weak = weak_from_this(), messagePtr](auto ec, std::size_t bytesTransferred) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream write failed."});
                        d.resolve(bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::
        PromiseTypeBind<Detail::PromiseTypeBindThen<std::string_view, std::size_t>, Detail::PromiseTypeBindFail<Error>>
        Client::read(std::chrono::seconds timeout)
    {
        return newPromise([this, timeout](Defer d) {
            withLowerLayerDo([timeout](auto& socket) {
                socket.expires_after(timeout);
            });
            withStreamDo([this, d = std::move(d)](auto& socket) mutable {
                struct Buf
                {
                    std::string data = std::string(4096, '\0');
                };
                auto buf = std::make_shared<Buf>();
                boost::asio::async_read(
                    socket,
                    boost::asio::buffer(buf->data),
                    [d = std::move(d), weak = weak_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        d.resolve(std::string_view{buf->data.data(), bytesTransferred}, bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void Client::doResolve(
        std::string const& host,
        std::string const& port,
        std::function<void(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)>
            onResolve)
    {
        withLowerLayerDo([&](auto& socket) {
            struct ResolveCtx
            {
                boost::asio::ip::tcp::resolver resolver;
                boost::asio::ip::tcp::resolver::query query;
            };
            auto resolveCtx = std::make_shared<ResolveCtx>(ResolveCtx{
                .resolver = boost::asio::ip::tcp::resolver{socket.get_executor()},
                .query =
                    boost::asio::ip::tcp::resolver::query{
                        host, port, boost::asio::ip::resolver_query_base::numeric_service},
            });
            resolveCtx->resolver.async_resolve(
                resolveCtx->query,
                [onResolve = std::move(onResolve), resolveCtx](boost::beast::error_code ec, auto results) mutable {
                    onResolve(ec, std::move(results));
                });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    namespace
    {
        class OnChunkHandler;
        struct OnChunkHeader;
        struct OnChunkBody;

        class SseContext : public std::enable_shared_from_this<SseContext>
        {
          public:
            SseContext(
                std::function<bool(std::string_view, std::string_view)> onEvent,
                std::function<void(std::optional<Error> const&)> onEndOfStream);

            void init();

            void enterReadCycle(std::shared_ptr<Client> client, std::chrono::seconds timeout);

            void parseChunk(std::string_view rawData);
            void onEndOfStream(std::optional<Error> const&);

          public:
            std::function<bool(std::string_view, std::string_view)> onEvent;
            boost::beast::flat_buffer headerBuffer;
            boost::beast::http::response_parser<boost::beast::http::empty_body> responseParser;

          private:
            std::function<void(std::optional<Error> const&)> onEndOfStream_;
            bool streamEndReached_;
            std::string messageBuffer_;
            std::size_t searchCursor_;
        };

        SseContext::SseContext(
            std::function<bool(std::string_view, std::string_view)> onEvent,
            std::function<void(std::optional<Error> const&)> onEndOfStream)
            : onEvent{std::move(onEvent)}
            , headerBuffer{}
            , responseParser{}
            , onEndOfStream_{std::move(onEndOfStream)}
            , streamEndReached_{false}
            , messageBuffer_{}
            , searchCursor_{0}
        {}

        void SseContext::onEndOfStream(std::optional<Error> const& err)
        {
            bool wasEndedBefore = streamEndReached_;
            streamEndReached_ = true;
            if (!wasEndedBefore)
                onEndOfStream_(err);
        }

        void SseContext::enterReadCycle(std::shared_ptr<Client> client, std::chrono::seconds timeout)
        {
            client->withLowerLayerDo([timeout](auto& socket) {
                socket.expires_after(timeout);
            });

            client->withStreamDo([&client, timeout, this](auto& socket) {
                if (streamEndReached_)
                    return;

                boost::asio::async_read_until(
                    socket,
                    headerBuffer,
                    "\n\n",
                    [weakClient = client->weak_from_this(), self = shared_from_this(), timeout](
                        boost::beast::error_code ec, std::size_t) mutable {
                        auto client = weakClient.lock();
                        if (!client)
                            return;

                        if (self->streamEndReached_)
                            return;

                        if (ec)
                            return self->onEndOfStream(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        self->parseChunk(std::string_view{
                            boost::asio::buffer_cast<char const*>(self->headerBuffer.data()),
                            self->headerBuffer.size()});

                        self->headerBuffer.consume(self->headerBuffer.size());

                        self->enterReadCycle(std::move(client), timeout);
                    });
            });
        }

        void SseContext::parseChunk(std::string_view rawData)
        {
            std::string data = std::string{rawData};
            using namespace boost::spirit::x3;

            std::vector<std::pair<std::string, std::string>> keyValuePairs;
            try
            {
                auto const parser = Detail::Parser::entries;
                auto iter = std::make_move_iterator(data.cbegin());
                const auto end = std::make_move_iterator(data.cend());

                bool success = parse(iter, end, parser, keyValuePairs);
                if (!success)
                {
                    return onEndOfStream(
                        Error{.error = boost::asio::error::no_recovery, .additionalInfo = "Error parsing chunk."});
                }

                std::string event;
                std::string eventData;

                for (auto const& [key, value] : keyValuePairs)
                {
                    if (key == "event")
                        event = value;
                    else if (key == "data")
                        eventData = value;
                }

                if (!onEvent(event, eventData))
                    onEndOfStream(std::nullopt);
            }
            catch (expectation_failure<std::string_view::const_iterator> const& e)
            {
                return onEndOfStream(Error{.error = boost::asio::error::no_recovery, .additionalInfo = e.what()});
            }
        }
    }
    // ##################################################################################################################
}