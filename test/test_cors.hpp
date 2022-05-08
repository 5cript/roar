#pragma once

#include "util/common_server_setup.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/curl/request.hpp>

#include <gtest/gtest.h>

#include <iostream>

namespace Roar::Tests
{
    class CorsListener
    {
      private:
        ROAR_MAKE_LISTENER(CorsListener);

        ROAR_GET(withCors)
        ({{
            .path = "/permissiveCors",
            .routeOptions =
                {
                    .cors = makePermissiveCorsSettings("get"),
                },
        }});
        ROAR_GET(withUnsecureCors)
        ({{
            .path = "/unsecurePermissiveCors",
            .routeOptions =
                {
                    .allowUnsecure = true,
                    .cors = makePermissiveCorsSettings("get"),
                },
        }});
        ROAR_GET(noCors)
        ({{
            .path = "/noCors",
            .routeOptions =
                {
                    .cors = std::nullopt,
                },
        }});
        ROAR_GET(withCustomCors)
        ({{
            .path = "/customCors",
            .routeOptions =
                {
                    .cors = CorsSettings{
                        .allowedOrigin =
                            [](auto const&) {
                                return "example.com";
                            },
                        .methodAllowSelection =
                            [](auto const&) {
                                return std::vector<std::string>{"GET"};
                            },
                        .headerAllowSelection =
                            [](auto const& headers) {
                                return headers;
                            },
                        .exposeHeaders =
                            {
                                boost::beast::http::field::content_encoding,
                            },
                        .allowCredentials = false,
                    },
                },
        }});
        ROAR_GET(noCorsByDefault)
        ({{
            .path = "/noCorsByDefault",
        }});

        void respondDefault(Session& session, EmptyBodyRequest&& req)
        {
            session.send(session.prepareResponse(req));
        }

      private:
        BOOST_DESCRIBE_CLASS(
            CorsListener,
            (),
            (),
            (),
            (roar_withCors, roar_noCors, roar_noCorsByDefault, roar_withCustomCors, roar_withUnsecureCors))
    };
    inline void CorsListener::withCors(Session& session, EmptyBodyRequest&& req)
    {
        respondDefault(session, std::move(req));
    }
    inline void CorsListener::noCors(Session& session, EmptyBodyRequest&& req)
    {
        respondDefault(session, std::move(req));
    }
    inline void CorsListener::withCustomCors(Session& session, EmptyBodyRequest&& req)
    {
        respondDefault(session, std::move(req));
    }
    inline void CorsListener::noCorsByDefault(Session& session, EmptyBodyRequest&& req)
    {
        respondDefault(session, std::move(req));
    }
    inline void CorsListener::withUnsecureCors(Session& session, EmptyBodyRequest&& req)
    {
        respondDefault(session, std::move(req));
    }

    class CorsTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        using CommonServerSetup::url;

        void SetUp() override
        {
            makeDefaultServer();
            makeSecureServer();
            server_->installRequestListener<CorsListener>();
            secureServer_->installRequestListener<CorsListener>();
        }
    };

    TEST_F(CorsTests, CorsIsDisabledByDefault)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/noCorsByDefault"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Allow-Origin"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Expose-Headers"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Allow-Headers"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Request-Method"));
    }

    TEST_F(CorsTests, CorsIsDisabledWhenSetToFalse)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/noCors"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Allow-Origin"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Expose-Headers"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Allow-Headers"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Request-Method"));
    }

    TEST_F(CorsTests, CorsAllowOriginIsKleeneWhenUnsetInRequest)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Origin"));
        EXPECT_EQ(headers["Access-Control-Allow-Origin"], "*");
    }

    TEST_F(CorsTests, CorsAllowOriginIsOriginOfRequest)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.setHeaderField("Origin", "bla.com").headerSink(headers).get(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Origin"));
        EXPECT_EQ(headers["Access-Control-Allow-Origin"], "bla.com");
    }

    TEST_F(CorsTests, CorsAllowHeadersIsKleeneWhenUnsetInRequest)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Headers"));
        EXPECT_EQ(headers["Access-Control-Allow-Headers"], "*");
    }

    TEST_F(CorsTests, CorsAllowHeadersIsWhatWasRequest)
    {
        const std::string allowedHeaders = "Accept-Encoding,Authorization";
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}
                       .headerSink(headers)
                       .setHeaderField("Access-Control-Request-Headers", allowedHeaders)
                       .get(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Headers"));
        EXPECT_EQ(headers["Access-Control-Allow-Headers"], allowedHeaders);
    }

    TEST_F(CorsTests, CorsCredentialsAreAllowed)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Credentials"));
        EXPECT_EQ(headers["Access-Control-Allow-Credentials"], "true");
    }

    TEST_F(CorsTests, ExposedHeadersAreNotSetIfNotSupplied)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/permissiveCors"));
        EXPECT_EQ(std::end(headers), headers.find("Access-Control-Expose-Headers"));
    }

    TEST_F(CorsTests, ExposedHeadersAreSet)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).get(url("/customCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Expose-Headers"));
        EXPECT_EQ(headers["Access-Control-Expose-Headers"], "Content-Encoding");
    }

    TEST_F(CorsTests, PreflightRequestRouteIsGenerated)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).options(url("/permissiveCors"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Origin"));
        EXPECT_EQ(headers["Access-Control-Allow-Origin"], "*");
    }

    TEST_F(CorsTests, CorsAllowMethodIsMethodOfRoute)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).options(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Methods"));
        EXPECT_EQ(headers["Access-Control-Allow-Methods"], "GET");
    }

    TEST_F(CorsTests, CorsAllowMethodIsMethodOfRouteEvenIfAnotherWasRequested)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}
                       .headerSink(headers)
                       .setHeaderField("Access-Control-Request-Method", "PUT")
                       .options(url("/permissiveCors"));
        ASSERT_NE(std::end(headers), headers.find("Access-Control-Allow-Methods"));
        EXPECT_EQ(headers["Access-Control-Allow-Methods"], "GET");
    }

    TEST_F(CorsTests, PreflightRequestOnEncryptedServerIsNotAccessibleUnencrypted)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).options(urlEncryptedServer("/permissiveCors"));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(CorsTests, PreflightRequestOnEncryptedServerIsAccessibleUnencryptedWhenAllowed)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).options(urlEncryptedServer("/unsecurePermissiveCors"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
    }
}