// must be top most to avoid winsock reinclude error.
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <roar/server.hpp>
#include <roar/routing/router.hpp>
#include <roar/dns/resolve.hpp>
#include <roar/session/session.hpp>
#include <roar/session/factory.hpp>

#include <optional>
#include <mutex>
#include <shared_mutex>

namespace Roar
{
    //##################################################################################################################
    struct Server::Implementation : public std::enable_shared_from_this<Server::Implementation>
    {
        boost::asio::ip::tcp::acceptor acceptor;
        std::optional<boost::asio::ssl::context> sslContext;
        boost::asio::ip::tcp::endpoint bindEndpoint;
        std::shared_mutex acceptorStopGuard;
        std::function<void(boost::system::error_code)> onAcceptAbort;
        std::unique_ptr<StandardResponseProvider> standardResponseProvider;
        std::shared_ptr<Router> router;
        std::function<void(Error&&)> onError;
        Factory sessionFactory;

        Implementation(
            boost::asio::any_io_executor& executor,
            std::optional<boost::asio::ssl::context> sslContext,
            std::function<void(Error&&)> onError,
            std::function<void(boost::system::error_code)> onAcceptAbort,
            std::unique_ptr<StandardResponseProvider> standardResponseProvider);

        void acceptOnce(int failCount);
    };
    //------------------------------------------------------------------------------------------------------------------
    Server::Implementation::Implementation(
        boost::asio::any_io_executor& executor,
        std::optional<boost::asio::ssl::context> sslContext,
        std::function<void(Error&&)> onError,
        std::function<void(boost::system::error_code)> onAcceptAbort,
        std::unique_ptr<StandardResponseProvider> standardResponseProvider)
        : acceptor{executor}
        , sslContext{std::move(sslContext)}
        , bindEndpoint{}
        , acceptorStopGuard{}
        , onAcceptAbort{std::move(onAcceptAbort)}
        , standardResponseProvider{std::move(standardResponseProvider)}
        , router{std::make_shared<Router>([this](Session& session, Request<boost::beast::http::empty_body> const& req) {
            session.send(this->standardResponseProvider->makeStandardResponse(
                session, req, boost::beast::http::status::not_found));
        })}
        , onError{std::move(onError)}
        , sessionFactory{this->sslContext, this->onError}
    {}
    //------------------------------------------------------------------------------------------------------------------
    void Server::Implementation::acceptOnce(int failCount)
    {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.get_executor());

        acceptor.async_accept(
            *socket, [self = shared_from_this(), socket, failCount](boost::system::error_code ec) mutable {
                if (ec == boost::asio::error::operation_aborted)
                    return;

                {
                    std::shared_lock lock{self->acceptorStopGuard};
                    if (!self->acceptor.is_open())
                        return;
                }

                if (!ec)
                {
                    self->sessionFactory.makeSession(std::move(*socket), self->router);
                    self->acceptOnce(0);
                    return;
                }
                else
                {
                    if (failCount >= 5)
                        return self->onAcceptAbort(ec);
                    self->acceptOnce(failCount + 1);
                }
            });
    }
    //##################################################################################################################
    Server::Server(ConstructionArguments constructionArgs)
        : impl_{std::make_shared<Implementation>(
              constructionArgs.executor,
              std::move(constructionArgs.sslContext),
              std::move(constructionArgs.onError),
              std::move(constructionArgs.onAcceptAbort),
              std::move(constructionArgs.standardResponseProvider))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    Server::~Server()
    {
        stop();
    }
    //------------------------------------------------------------------------------------------------------------------
    boost::leaf::result<void> Server::start(unsigned short port, std::string const& host)
    {
        return start(Dns::resolveSingle(impl_->acceptor.get_executor(), host, port));
    }
    //------------------------------------------------------------------------------------------------------------------
    boost::leaf::result<void> Server::start(boost::asio::ip::tcp::endpoint const& bindEndpoint)
    {
        stop();
        boost::system::error_code ec;
        impl_->bindEndpoint = bindEndpoint;

        impl_->acceptor.open(impl_->bindEndpoint.protocol(), ec);
        if (ec)
            return boost::leaf::new_error("Could not open http server acceptor.", ec);

        impl_->acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            return boost::leaf::new_error("Could not configure socket to reuse address.", ec);

        impl_->acceptor.bind(impl_->bindEndpoint, ec);
        if (ec)
            return boost::leaf::new_error("Could not bind socket.", ec);

        impl_->acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            return boost::leaf::new_error("Could not listen on socket.", ec);

        impl_->acceptOnce(0);
        return {};
    }
    //------------------------------------------------------------------------------------------------------------------
    void Server::stop()
    {
        std::scoped_lock lock{impl_->acceptorStopGuard};
        impl_->acceptor.close();
    }
    //------------------------------------------------------------------------------------------------------------------
    void Server::addRequestListenerToRouter(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes)
    {
        impl_->router->addRoutes(std::move(routes));
    }
    //------------------------------------------------------------------------------------------------------------------
    Server::Server(Server&&) = default;
    //------------------------------------------------------------------------------------------------------------------
    Server& Server::operator=(Server&&) = default;
    //##################################################################################################################
}