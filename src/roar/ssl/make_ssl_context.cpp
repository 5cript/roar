#include <roar/ssl/make_ssl_context.hpp>
#include <roar/utility/overloaded.hpp>

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

        std::visit(
            overloaded{
                [&](std::string_view key) {
                    sslContext.use_certificate_chain(boost::asio::buffer(key.data(), key.size()));
                },
                [&](std::filesystem::path const& path) {
                    sslContext.use_certificate_chain_file(path.string());
                },
            },
            settings.certificate);

        std::visit(
            overloaded{
                [&](std::string_view key) {
                    sslContext.use_private_key(
                        boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);
                },
                [&](std::filesystem::path const& path) {
                    sslContext.use_private_key_file(path.string(), boost::asio::ssl::context::file_format::pem);
                }},
            settings.privateKey);

        if (!settings.diffieHellmanParameters.empty())
        {
            sslContext.use_tmp_dh(
                boost::asio::buffer(settings.diffieHellmanParameters.data(), settings.diffieHellmanParameters.size()));
        }

        return sslContext;
    }
}