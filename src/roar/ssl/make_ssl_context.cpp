#include <roar/ssl/make_ssl_context.hpp>
#include <roar/utility/overloaded.hpp>

namespace Roar
{
    void initializeServerSslContext(SslServerContext& ctx)
    {
        ctx.ctx.set_password_callback(
            [password = std::string{ctx.password}](std::size_t, boost::asio::ssl::context_base::password_purpose) {
                return password;
            });

        ctx.ctx.set_options(
            boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

        std::visit(
            overloaded{
                [&](std::string const& key) {
                    ctx.ctx.use_certificate_chain(boost::asio::buffer(key.data(), key.size()));
                },
                [&](std::filesystem::path const& path) {
                    ctx.ctx.use_certificate_chain_file(path.string());
                },
            },
            ctx.certificate);

        std::visit(
            overloaded{
                [&](std::string const& key) {
                    ctx.ctx.use_private_key(
                        boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);
                },
                [&](std::filesystem::path const& path) {
                    ctx.ctx.use_private_key_file(path.string(), boost::asio::ssl::context::file_format::pem);
                }},
            ctx.privateKey);

        if (!ctx.diffieHellmanParameters.empty())
        {
            ctx.ctx.use_tmp_dh(
                boost::asio::buffer(ctx.diffieHellmanParameters.data(), ctx.diffieHellmanParameters.size()));
        }
    }

    boost::asio::ssl::context makeSslContext(const std::string& certificate, const std::string& privateKey)
    {
        SslServerContext ctx;
        ctx.certificate = certificate;
        ctx.privateKey = privateKey;
        initializeServerSslContext(ctx);
        return std::move(ctx.ctx);
    }
}