#include <roar/ssl/make_ssl_context.hpp>

namespace Roar
{
    boost::asio::ssl::context makeSslContext(SslContextCreationParameters settings)
    {
        boost::asio::ssl::context sslContext{settings.method};

        sslContext.set_password_callback(
            [password = settings.password](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                return std::string{password};
            });

        sslContext.set_options(
            boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

        sslContext.use_certificate_chain(boost::asio::buffer(settings.certificate.data(), settings.certificate.size()));

        sslContext.use_private_key(
            boost::asio::buffer(settings.privateKey.data(), settings.privateKey.size()),
            boost::asio::ssl::context::file_format::pem);

        if (!settings.diffieHellmanParameters.empty())
        {
            sslContext.use_tmp_dh(
                boost::asio::buffer(settings.diffieHellmanParameters.data(), settings.diffieHellmanParameters.size()));
        }

        return sslContext;
    }
}