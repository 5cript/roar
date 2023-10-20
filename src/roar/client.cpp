#include <roar/client.hpp>

#include <boost/fusion/include/at_c.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>

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

            const auto key = x3::rule<struct KeyTag, std::string>{"key"} = +(char_ - ':');
            const auto value = x3::rule<struct ValueTag, std::string>{"value"} = +(char_ - '\n');
            const auto newLine = x3::rule<struct NewLineTag, std::string>{"newLine"} = char_('\n');
            const auto whitespace = x3::rule<struct WhitespaceTag, std::string>{"whitespace"} =
                char_(' ') | char_('\t');

            const auto entry = x3::rule<struct EntryTag, std::pair<std::string, std::string>>{"entry"} =
                key[([](auto& ctx) {
                    x3::traits::move_to(x3::_attr(ctx), x3::_val(ctx).first);
                })] >>
                ':' >> *whitespace >> value[([](auto& ctx) {
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
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL_NO_MOVE(Client);
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
            withStreamDo([this, timeout, d = std::move(d)](auto& socket) mutable {
                struct Buf
                {
                    std::string data = std::string(4096, '\0');
                };
                auto buf = std::make_shared<Buf>();
                socket.async_read_some(
                    boost::asio::buffer(buf->data, buf->data.size()),
                    [d = std::move(d), weak = weak_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        d.resolve(std::string_view{buf->data}, bytesTransferred);
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
        class OnChunkHeader;
        class OnChunkBody;

        class SseContext : public std::enable_shared_from_this<SseContext>
        {
          public:
            SseContext(
                std::function<bool(std::uint64_t size, std::string_view)> onHeader,
                std::function<bool(std::string_view, std::string_view)> onEvent,
                std::function<void(std::optional<Error> const&)> onEndOfStream);

            void init();

            void enterReadCycle(std::shared_ptr<Client> client, std::chrono::seconds timeout);

            std::size_t parseChunk(std::uint64_t remain, std::string_view rawData);
            void onEndOfStream(std::optional<Error> const&);

          public:
            std::function<bool(std::uint64_t size, std::string_view)> onHeader;
            std::function<bool(std::string_view, std::string_view)> onEvent;
            boost::beast::flat_buffer headerBuffer;
            boost::beast::http::response_parser<boost::beast::http::empty_body> responseParser;

          private:
            std::unique_ptr<OnChunkHeader> onChunkHeader_;
            std::unique_ptr<OnChunkBody> onChunkBody_;
            std::function<void(std::optional<Error> const&)> onEndOfStream_;
            bool streamEndReached_;
            std::string messageBuffer_;
        };

        class OnChunkHandler
        {
          public:
            void setWeakContext(std::weak_ptr<SseContext> weakCtx);
            virtual ~OnChunkHandler() = default;

          protected:
            std::weak_ptr<SseContext> weakCtx_;
        };

        struct OnChunkHeader : public OnChunkHandler
        {
            void operator()(std::uint64_t size, std::string_view extensions, boost::beast::error_code& error);
        };

        struct OnChunkBody : public OnChunkHandler
        {
            std::size_t operator()(std::uint64_t remain, std::string_view body, boost::beast::error_code& error);
        };

        void OnChunkHandler::setWeakContext(std::weak_ptr<SseContext> weakCtx)
        {
            weakCtx_ = std::move(weakCtx);
        }

        void OnChunkHeader::operator()(std::uint64_t size, std::string_view ext, boost::beast::error_code& error)
        {
            auto ctx = weakCtx_.lock();
            if (!ctx)
            {
                error = boost::asio::error::no_recovery;
                return;
            }

            if (!ctx->onHeader(size, ext))
                return ctx->onEndOfStream(std::nullopt);

            if (size == 0)
                return ctx->onEndOfStream(std::nullopt);
        }

        std::size_t
        OnChunkBody::operator()(std::uint64_t remain, std::string_view body, boost::beast::error_code& error)
        {
            auto ctx = weakCtx_.lock();
            if (!ctx)
            {
                error = boost::asio::error::no_recovery;
                return body.size();
            }

            return ctx->parseChunk(remain, body);
        }

        SseContext::SseContext(
            std::function<bool(std::uint64_t size, std::string_view)> onHeader,
            std::function<bool(std::string_view, std::string_view)> onEvent,
            std::function<void(std::optional<Error> const&)> onEndOfStream)
            : onHeader{std::move(onHeader)}
            , onEvent{std::move(onEvent)}
            , headerBuffer{}
            , responseParser{}
            , onChunkHeader_{std::make_unique<OnChunkHeader>()}
            , onChunkBody_{std::make_unique<OnChunkBody>()}
            , onEndOfStream_{std::move(onEndOfStream)}
            , streamEndReached_{false}
            , messageBuffer_{}
        {}

        void SseContext::onEndOfStream(std::optional<Error> const& err)
        {
            streamEndReached_ = true;
            onEndOfStream_(err);
        }

        void SseContext::init()
        {
            onChunkHeader_->setWeakContext(weak_from_this());
            onChunkBody_->setWeakContext(weak_from_this());
            responseParser.on_chunk_header(*onChunkHeader_);
            responseParser.on_chunk_body(*onChunkBody_);
        }

        void SseContext::enterReadCycle(std::shared_ptr<Client> client, std::chrono::seconds timeout)
        {
            client->withLowerLayerDo([&](auto& socket) {
                socket.expires_after(timeout);
            });

            client->withStreamDo([client, timeout, self = shared_from_this()](auto& socket) {
                boost::beast::http::async_read_some(
                    socket,
                    self->headerBuffer,
                    self->responseParser,
                    [self, client, timeout](auto ec, auto bytesTransferred) {
                        if (ec)
                            return self->onEndOfStream(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        if (self->streamEndReached_)
                            return;
                        self->enterReadCycle(client, timeout);
                    });
            });
        }

        std::size_t SseContext::parseChunk(std::uint64_t remain, std::string_view rawData)
        {
            auto const missing = remain - static_cast<std::uint64_t>(rawData.size());
            messageBuffer_.append(rawData);

            // We need to wait for more data
            if (missing > 0)
                return rawData.size();

            auto actualParse = [this](std::string& data) {
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
                    std::string data;

                    for (auto const& [key, value] : keyValuePairs)
                    {
                        if (key == "event")
                            event = value;
                        else if (key == "data")
                            data = value;
                    }

                    if (!onEvent(event, data))
                        onEndOfStream(std::nullopt);
                }
                catch (expectation_failure<std::string_view::const_iterator> const& e)
                {
                    return onEndOfStream(Error{.error = boost::asio::error::no_recovery, .additionalInfo = e.what()});
                }
            };

            actualParse(messageBuffer_);
            messageBuffer_.clear();
            return rawData.size();
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<
        Detail::PromiseTypeBindThen<
            Detail::PromiseReferenceWrap<boost::beast::http::response_parser<boost::beast::http::empty_body>>,
            Detail::PromiseReferenceWrap<bool>>,
        Detail::PromiseTypeBindFail<Error>>
    Client::readServerSentEvents(
        std::function<bool(std::string_view, std::string_view)> onEvent,
        std::function<void(std::optional<Error> const&)> onEndOfStream,
        std::function<bool(std::uint64_t size, std::string_view)> onHeader,
        std::chrono::seconds timeout)
    {
        auto ctx = std::make_shared<SseContext>(std::move(onHeader), std::move(onEvent), std::move(onEndOfStream));
        return newPromise([this, timeout, ctx](Defer d) mutable {
            ctx->init();

            withLowerLayerDo([&](auto& socket) {
                socket.expires_after(timeout);
            });
            withStreamDo([d = std::move(d), timeout, ctx, this](auto& socket) mutable {
                boost::beast::http::async_read_header(
                    socket,
                    ctx->headerBuffer,
                    ctx->responseParser,
                    [weak = weak_from_this(), ctx, timeout, d = std::move(d)](
                        boost::beast::error_code ec, std::size_t) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        bool shallContinue = true;
                        d.resolve(Detail::ref(ctx->responseParser), Detail::ref(shallContinue));

                        if (!shallContinue)
                            return;

                        ctx->enterReadCycle(self, timeout);
                    });
            });
        });
    }
    // ##################################################################################################################
}