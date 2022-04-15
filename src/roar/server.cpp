// must be top most to avoid winsock reinclude error.
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

#include <roar/server.hpp>
#include <roar/dns/resolve.hpp>

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

        Implementation(boost::asio::any_io_executor& executor, std::optional<boost::asio::ssl::context> sslContext);

        void acceptOnce();
    };
    //------------------------------------------------------------------------------------------------------------------
    Server::Implementation::Implementation(
        boost::asio::any_io_executor& executor,
        std::optional<boost::asio::ssl::context> sslContext)
        : acceptor{executor}
        , sslContext{std::move(sslContext)}
        , bindEndpoint{}
    {}
    //------------------------------------------------------------------------------------------------------------------
    void Server::Implementation::acceptOnce()
    {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.get_executor());

        acceptor.async_accept(*socket, [self = shared_from_this(), socket](boost::system::error_code ec) mutable {
            if (ec == boost::asio::error::operation_aborted)
                return;

            {
                std::shared_lock lock{self->acceptorStopGuard};

                if (!self->acceptor.is_open())
                    return;
            }

            // FIXME: not every error should continue accepting:
            if (ec)
                return self->acceptOnce();
            self->acceptOnce();
        });
    }
    //##################################################################################################################
    Server::Server(boost::asio::any_io_executor& executor)
        : impl_{std::make_unique<Implementation>(executor, std::nullopt)}
    {}
    Server::Server(boost::asio::any_io_executor& executor, boost::asio::ssl::context&& sslContext)
        : impl_{std::make_unique<Implementation>(executor, std::move(sslContext))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    Server::~Server()
    {
        stop();
    }
    //------------------------------------------------------------------------------------------------------------------
    boost::leaf::result<void> Server::start(std::string const& host, unsigned short port)
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

        impl_->acceptOnce();
        return {};
    }
    //------------------------------------------------------------------------------------------------------------------
    void Server::stop()
    {
        std::scoped_lock lock{impl_->acceptorStopGuard};
        impl_->acceptor.close();
    }
    //------------------------------------------------------------------------------------------------------------------
    Server::Server(Server&&) = default;
    //------------------------------------------------------------------------------------------------------------------
    Server& Server::operator=(Server&&) = default;
    //##################################################################################################################
}