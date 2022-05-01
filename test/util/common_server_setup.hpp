#pragma once

#include "../resources/keys.hpp"

#include <roar/server.hpp>
#include <roar/utility/url.hpp>
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
            : pool_{2}
            , executor_{pool_.executor()}
            , errors_{}
            , server_{}
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

        void makeSecureServer()
        {
            secureServer_ = std::make_unique<Roar::Server>(Roar::Server::ConstructionArguments{
                .executor = executor_,
                .sslContext = makeSslContext({
                    .certificate = certificateForTests,
                    .privateKey = keyForTests,
                    .password = keyPassphrase,
                }),
                .onError =
                    [this](Roar::Error&& err) {
                        errors_.push_back(std::move(err));
                    },
            });
            secureServer_->start();
        }

        std::string urlImpl(Roar::Server& server, std::string const& path, UrlParams params = {}) const
        {
            const auto url =
                Url{
                    .scheme = params.secure ? "https" : "http",
                    .remote = {.host = "localhost", .port = server.getLocalEndpoint().port()},
                    .path = path,
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
    };
}