#pragma once

#include "../util/common_server_setup.hpp"
#include "../util/temporary_directory.hpp"
#include "../util/common_listeners.hpp"

#include <roar/utility/base64.hpp>
#include <roar/curl/global_curl_context.hpp>
#include <roar/curl/request.hpp>
#include <roar/curl/sink.hpp>
#include <roar/curl/sources/file_source.hpp>
#include <roar/curl/sources/string_source.hpp>

#include <gtest/gtest.h>

namespace Roar::Tests
{
    using Request = Roar::Curl::Request;
    namespace http = boost::beast::http;
    using namespace std::literals;

    class CustomSink : public Curl::Sink
    {
      public:
        void feed(char const* data, std::size_t amount) override
        {
            m_accum.append(data, amount);
        }

        std::string m_accum;
    };

    class CurlRequestTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        using CommonServerSetup::url;

        void SetUp() override
        {
            makeDefaultServer();
            server_->installRequestListener<SimpleRoutes>();
            server_->installRequestListener<HeaderReflector>();
        }

        void TearDown() override
        {
            this->stop();
        }

        std::string makeTestFile(std::filesystem::path const& name)
        {
            const int fileSize = 70000; // big enough to likely not fit a buffer. also not 2^X.
            const auto path = m_tempDir.path() / name;

            std::string compareAgainst = "";
            {
                std::ofstream writer{path};
                for (int i = 0; i != fileSize; ++i)
                {
                    char c = 'A' + i % 26;
                    compareAgainst.push_back(c);
                    writer.put(c);
                }
            }
            return compareAgainst;
        }

      protected:
        Curl::GlobalCurlContext m_curlContext;
        TemporaryDirectory m_tempDir{TEST_TEMPORARY_DIRECTORY, true};
    };

    TEST_F(CurlRequestTests, CanMakeSimpleGetRequest)
    {
        auto response = Request{}.get(url("/index.txt"));
        EXPECT_EQ(response.code(), http::status::ok) << response.result();
    }

    TEST_F(CurlRequestTests, CanRetrieveBodyWithStringSink)
    {
        std::string body;
        Request{}.sink(body).get(url("/index.txt"));
        EXPECT_EQ(body, "Hello");
    }

    TEST_F(CurlRequestTests, CanRetrieveBodyWithCustomSink)
    {
        CustomSink sink;
        Request{}.sink(sink).get(url("/index.txt"));
        EXPECT_EQ(sink.m_accum, "Hello");
    }

    TEST_F(CurlRequestTests, CanRetrieveBodyToFile)
    {
        const auto path = m_tempDir.path() / "index.txt";
        Request{}.sink(path).get(url("/index.txt"));
        std::ifstream reader{path, std::ios_base::binary};
        std::stringstream sstr;
        sstr << reader.rdbuf();
        EXPECT_EQ(sstr.str(), "Hello");
    }

    TEST_F(CurlRequestTests, CanRetrieveToMultipleSinks)
    {
        const auto path = m_tempDir.path() / "index.txt";
        std::string body;
        Request{}.sink(body).sink(path).get(url("/index.txt"));
        std::ifstream reader{path, std::ios_base::binary};
        std::stringstream sstr;
        sstr << reader.rdbuf();
        EXPECT_EQ(sstr.str(), "Hello");
        EXPECT_EQ(body, "Hello");
    }

    TEST_F(CurlRequestTests, CanGetResponseCode)
    {
        const auto response = Request{}.get(url("/index.txt"));
        EXPECT_EQ(response.code(), boost::beast::http::status::ok);
    }

    TEST_F(CurlRequestTests, CanGetSizeOfDownload)
    {
        const auto response = Request{}.get(url("/index.txt"));
        EXPECT_EQ(response.sizeOfDownload(), 5);
    }

    TEST_F(CurlRequestTests, CanUseBasicAuth)
    {
        nlohmann::json body;
        const auto response =
            Request{}.basicAuth("User", "Password").verifyPeer(false).verifyHost(false).sink(body).get(url("/headers"));
        EXPECT_EQ(body["Authorization"], "Basic "s + base64Encode("User:Password"));
    }

    TEST_F(CurlRequestTests, CanSetAcceptedEncoding)
    {
        nlohmann::json body;
        const auto response = Request{}.acceptEncoding("application/json").sink(body).get(url("/headers"));
        EXPECT_EQ(body["Accept-Encoding"], "application/json");
    }

    TEST_F(CurlRequestTests, CanSetCustomHeader)
    {
        nlohmann::json body;
        Request{}.setHeaderFields({{"Custom", "Value"}}).sink(body).get(url("/headers"));
        EXPECT_EQ(body["Custom"], "Value");
    }

    TEST_F(CurlRequestTests, CanMakePutRequest)
    {
        std::string body;
        const auto result = Request{}.source("Hello").sink(body).put(url("/putHere"));
        EXPECT_EQ(body, "Hello");
    }

    TEST_F(CurlRequestTests, CanMakePutRequestWithSimpleFileSource)
    {
        const std::string fileName = "bla.txt";
        const auto compareAgainst = makeTestFile(fileName);
        std::string body;
        const auto result = Request{}.source(m_tempDir.path() / fileName).sink(body).put(url("/putHere"));
        EXPECT_EQ(body, compareAgainst);
    }

    TEST_F(CurlRequestTests, CanMakePostRequest)
    {
        std::string body;
        const auto result = Request{}.source("Hello").sink(body).post(url("/postHere"));
        EXPECT_EQ(body, "Hello");
    }

    TEST_F(CurlRequestTests, CanMakeDeleteRequest)
    {
        const auto result = Request{}.delete_(url("/deleteHere"));
        EXPECT_EQ(result.code(), boost::beast::http::status::ok);
    }

    TEST_F(CurlRequestTests, CanMakeOptionsRequest)
    {
        const auto result = Request{}.options(url("/optionsHere"));
        EXPECT_EQ(result.code(), boost::beast::http::status::ok);
    }

    TEST_F(CurlRequestTests, CanMakeHeadRequest)
    {
        const auto result = Request{}.head(url("/headHere"));
        EXPECT_EQ(result.code(), boost::beast::http::status::ok);
    }

    TEST_F(CurlRequestTests, CanUseFileAsSource)
    {
        const auto compareAgainst = makeTestFile("bla.txt");

        std::string body;
        const auto result =
            Request{}.emplaceSource<Curl::FileSource>(m_tempDir.path() / "bla.txt").sink(body).post(url("/postHere"));
        EXPECT_EQ(body, compareAgainst);
    }

    TEST_F(CurlRequestTests, CanSetContentType)
    {
        std::string body;
        const auto result = Request{}.source("").contentType("application/json").sink(body).get(url("/headers"));
        auto j = nlohmann::json::parse(body);
        EXPECT_EQ(j["Content-Type"], "application/json");
    }

    TEST_F(CurlRequestTests, CanSetContentTypeAndHeaders)
    {
        nlohmann::json body;
        const auto result = Request{}
                                .source("")
                                .contentType("application/json")
                                .setHeaderFields({{"X", "Y"}})
                                .sink(body)
                                .get(url("/headers"));
        EXPECT_EQ(body["Content-Type"], "application/json");
        EXPECT_EQ(body["X"], "Y");
    }

    TEST_F(CurlRequestTests, CanRegisterHeaderReader)
    {
        std::unordered_map<std::string, std::string> headers;
        const auto result = Request{}
                                .source("")
                                .onHeaderValue([&headers](std::string const& key, std::string const& value) {
                                    headers[key] = value;
                                })
                                .get(url("/index.txt"));
        EXPECT_GE(headers.size(), 1);
        EXPECT_EQ(headers["Content-Type"], "text/plain");
    }

    TEST_F(CurlRequestTests, CanPerformCustomRequest)
    {
        std::string body;
        const auto result = Request{}.sink(body).custom("GET", url("/index.txt"));
        EXPECT_EQ(body, "Hello");
    }

    TEST_F(CurlRequestTests, CanPerformBodylessPutViaCustomRequest)
    {
        const auto result = Request{}.custom("PUT", url("/putHereNothing"));
        EXPECT_EQ(result.code(), boost::beast::http::status::no_content);
    }
}