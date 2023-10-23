#pragma once

#include "../resources/keys.hpp"
#include "temporary_directory.hpp"

#include <roar/server.hpp>
#include <roar/client.hpp>
#include <roar/url/url.hpp>
#include <roar/ssl/make_ssl_context.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>

#include <memory>

namespace Roar::Tests
{
    struct UrlParams
    {
        bool secure = false;
    };

    class CommonServerSetup
    {
      public:
        CommonServerSetup()
            : pool_{4}
            , executor_{pool_.executor()}
            , errors_{}
            , server_{}
            , tmpDir_{std::filesystem::current_path(), true}
        {}

        void makeDefaultServer()
        {
            server_ = std::make_unique<Roar::Server>(Roar::Server::ConstructionArguments{
                .executor = executor_,
                .onError =
                    [this](Roar::Error&& err) {
                        errors_.push_back(std::move(err));
                    },
            });
            server_->start();
        }

        std::pair<std::filesystem::path, std::filesystem::path> generateCertAndKey() const
        {
            std::filesystem::path certFile = tmpDir_.path() / "example.com.crt";
            std::filesystem::path keyFile = tmpDir_.path() / "example.com.key";
            std::stringstream cmd;
            cmd << "openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 -nodes -keyout " << keyFile << " -out "
                << certFile
                << " -subj \"/CN=example.com\" -addext "
                   "\"subjectAltName=DNS:example.com,DNS:*.example.com,IP:127.0.0.1\"";
            system(cmd.str().c_str());
            return {certFile, keyFile};
        }

        void makeSecureServer(bool freshKeys = false)
        {
            auto ctx = [this, freshKeys]() {
                if (freshKeys)
                {
                    const auto [certFile, keyFile] = generateCertAndKey();
                    return SslServerContext{
                        .certificate = certFile,
                        .privateKey = keyFile,
                    };
                }
                else
                {
                    return SslServerContext{
                        .certificate = std::string{certificateForTests},
                        .privateKey = std::string{keyForTests},
                        .password = std::string{keyPassphrase},
                    };
                }
            }();
            initializeServerSslContext(ctx);

            secureServer_ = std::make_unique<Roar::Server>(Roar::Server::ConstructionArguments{
                .executor = executor_,
                .sslContext = std::move(ctx),
                .onError =
                    [this](Roar::Error&& err) {
                        errors_.push_back(std::move(err));
                    },
            });
            secureServer_->start();
        }

        std::shared_ptr<Client>
        makeClient(std::string const& scheme = "http", std::optional<Client::SslOptions> options = std::nullopt)
        {
            if (scheme == "http")
                return std::make_shared<Client>(Client::ConstructionArguments{.executor = executor_});
            else if (scheme == "https")
            {
                if (!options)
                {
                    options = Client::SslOptions{
                        .sslContext = boost::asio::ssl::context{boost::asio::ssl::context::tlsv12_client},
                        .sslVerifyMode = boost::asio::ssl::verify_none,
                    };
                }

                return std::make_shared<Client>(Client::ConstructionArguments{
                    .executor = executor_,
                    .sslOptions = std::move(options),
                });
            }
            else
                throw std::runtime_error{"Unknown scheme: " + scheme};
        }

        std::string urlImpl(Roar::Server& server, std::string const& path, UrlParams params = {}) const
        {
            const auto url =
                Url{
                    .scheme = params.secure ? "https" : "http",
                    .authority =
                        {
                            .remote = {.host = "localhost", .port = server.getLocalEndpoint().port()},
                        },
                    .path = Url::parsePath(path).value(),
                }
                    .toString();
            return url;
        }

        std::string url(std::string const& path, UrlParams params = {}) const
        {
            return urlImpl(*server_, path, params);
        }

        std::string urlEncryptedServer(std::string const& path, UrlParams params = {}) const
        {
            return urlImpl(*secureServer_, path, params);
        }

        void stop()
        {
            server_->stop();
            pool_.stop();
            pool_.join();
        }

      protected:
        boost::asio::thread_pool pool_;
        boost::asio::any_io_executor executor_;
        std::vector<Roar::Error> errors_;
        std::unique_ptr<Roar::Server> server_;
        std::unique_ptr<Roar::Server> secureServer_;
        TemporaryDirectory tmpDir_;
    };
}