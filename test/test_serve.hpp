#pragma once

#include <roar/curl/request.hpp>

#include "util/common_listeners.hpp"
#include "util/common_server_setup.hpp"
#include "util/temporary_directory.hpp"

#include <gtest/gtest.h>

#include <fstream>

namespace Roar::Tests
{
    namespace http = boost::beast::http;
    using namespace Roar::Literals;

    class ServingListener
    {
      public:
        constexpr static char const* DummyFileContent = "File Contents";

      public:
        ROAR_MAKE_LISTENER(ServingListener);
        ServingListener()
            : tempDir_{TEST_TEMPORARY_DIRECTORY}
            , alternativeSupplier_{tempDir_}
        {
            std::filesystem::create_directory(pathSupplier() / "nonEmptyDir");
            std::filesystem::create_directory(pathSupplier() / "emptyDir");
            createDummyFile(pathSupplier() / "nonEmptyDir" / "file.txt");
            createDummyFile(pathSupplier() / "file.txt");
            createDummyFile(pathSupplier() / "index.html");
            std::filesystem::create_directory(pathSupplier() / "a");
            std::filesystem::create_directory(pathSupplier() / "a" / "b");
            std::filesystem::create_directory(pathSupplier() / "a" / "b" / "c");
        }
        std::filesystem::path pathSupplier() const
        {
            return tempDir_;
        }

      private:
        TemporaryDirectory tempDir_;
        std::filesystem::path alternativeSupplier_;

      private: // private member functions
        std::filesystem::path pathSupplier2()
        {
            return tempDir_;
        }
        std::filesystem::path pathDeep()
        {
            return tempDir_.path() / "a" / "b" / "c";
        }

        void createDummyFile(std::filesystem::path const& where)
        {
            std::ofstream writer{where, std::ios_base::binary};
            writer << DummyFileContent;
        }

      private: // routes
        ROAR_GET(precendenceCheck)("/1/bla");
        ROAR_GET(precendenceCheckRegex)("/1/(.+)"_rgx);

        ROAR_SERVE(serve1)("/1", &ServingListener::alternativeSupplier_);
        ROAR_SERVE(serve2)("/2", &ServingListener::pathSupplier);
        ROAR_SERVE(serveDeny)("/deny", &ServingListener::pathSupplier2);
        ROAR_SERVE(serveCustomDeny)("/customDeny", &ServingListener::pathSupplier);
        ROAR_SERVE(deep)("/deep", &ServingListener::pathDeep);
        ROAR_SERVE(allAllowed)
        ({
            {.path = "/allAllowed", .routeOptions = {.allowUnsecure = true}},
            {
                .allowDownload = true,
                .allowUpload = true,
                .allowOverwrite = true,
                .allowDelete = true,
                .allowDeleteOfNonEmptyDirectories = true,
                .allowListing = true,
                .pathProvider = &ServingListener::pathSupplier,
            },
        });
        ROAR_SERVE(nothingAllowed)
        ({
            {.path = "/nothingAllowed"},
            {
                .allowDownload = false,
                .allowUpload = false,
                .allowOverwrite = false,
                .allowDelete = false,
                .allowDeleteOfNonEmptyDirectories = false,
                .allowListing = false,
                .pathProvider = &ServingListener::pathSupplier,
            },
        });
        ROAR_SERVE(deleteAllowedButNotDirectories)
        ({
            {.path = "/deleteAllowedButNotDirectories"},
            {
                .allowDownload = false,
                .allowUpload = false,
                .allowDelete = true,
                .allowDeleteOfNonEmptyDirectories = false,
                .allowListing = false,
                .pathProvider = &ServingListener::pathSupplier,
            },
        });
        ROAR_SERVE(overwriteNotAllowed)
        ({
            {.path = "/overwriteNotAllowed"},
            {
                .allowDownload = false,
                .allowUpload = true,
                .allowOverwrite = false,
                .allowDelete = false,
                .allowDeleteOfNonEmptyDirectories = false,
                .allowListing = false,
                .pathProvider = &ServingListener::pathSupplier,
            },
        });
        ROAR_SERVE(nothingIsAllowedButIsOverruled)
        ({
            {.path = "/nothingIsAllowedButIsOverruled"},
            {
                .allowDownload = false,
                .allowUpload = false,
                .allowOverwrite = false,
                .allowDelete = false,
                .allowDeleteOfNonEmptyDirectories = false,
                .allowListing = false,
                .pathProvider = &ServingListener::pathSupplier,
            },
        });

      private: // reflection.
        BOOST_DESCRIBE_CLASS(
            ServingListener,
            (),
            (),
            (),
            (roar_precendenceCheck,
             roar_precendenceCheckRegex,
             roar_serve1,
             roar_serve2,
             roar_serveDeny,
             roar_serveCustomDeny,
             roar_allAllowed,
             roar_nothingIsAllowedButIsOverruled,
             roar_nothingAllowed,
             roar_deleteAllowedButNotDirectories,
             roar_overwriteNotAllowed,
             roar_deep))
    };
    inline void ServingListener::precendenceCheck(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<http::string_body>(req)->body("string").status(status::ok).commit();
    }
    inline void ServingListener::precendenceCheckRegex(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<http::string_body>(req)->body("regex").status(status::ok).commit();
    }
    inline Roar::ServeDecision ServingListener::serve1(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::serve2(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::serveDeny(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Deny;
    }

    inline Roar::ServeDecision ServingListener::allAllowed(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::nothingAllowed(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::deleteAllowedButNotDirectories(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::overwriteNotAllowed(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::serveCustomDeny(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        session.send<http::empty_body>(request)->status(http::status::bad_request).commit();
        return Roar::ServeDecision::Handled;
    }

    inline Roar::ServeDecision ServingListener::deep(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        return Roar::ServeDecision::Continue;
    }

    inline Roar::ServeDecision ServingListener::nothingIsAllowedButIsOverruled(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus,
        Roar::ServeOptions<ServingListener>& options)
    {
        options.allowDownload = true;
        return Roar::ServeDecision::Continue;
    }

    class ServeTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        using CommonServerSetup::url;

        void SetUp() override
        {
            makeDefaultServer();
            makeSecureServer();
            listener_ = server_->installRequestListener<ServingListener>();
            listenerSecure_ = secureServer_->installRequestListener<ServingListener>();
        }

      protected:
        std::shared_ptr<ServingListener> listener_;
        std::shared_ptr<ServingListener> listenerSecure_;
    };

    TEST_F(ServeTests, CanMakeHeadRequest)
    {
        std::unordered_map<std::string, std::string> headers;
        const auto res = Curl::Request{}.headerSink(headers).head(url("/1"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
    }

    TEST_F(ServeTests, CanotMakeHeadRequestWhenDownloadIsNotAllowed)
    {
        std::unordered_map<std::string, std::string> headers;
        const auto res = Curl::Request{}.headerSink(headers).head(url("/nothingAllowed"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }

    TEST_F(ServeTests, StringPathsTakePrecedence)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/1/bla"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, "string");
    }

    TEST_F(ServeTests, RegexPathsTakePrecedence)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/1/blub"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, "regex");
    }

    TEST_F(ServeTests, OptionsRequestReturnsPermittedMethods)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).options(url("/nothingAllowed"));
        EXPECT_EQ(res.code(), boost::beast::http::status::no_content);
        EXPECT_EQ(headers.at("Allow"), "OPTIONS");

        res = Curl::Request{}.headerSink(headers).options(url("/allAllowed"));
        EXPECT_EQ(res.code(), boost::beast::http::status::no_content);
        EXPECT_EQ(headers.at("Allow"), "OPTIONS, GET, HEAD, PUT, DELETE");
    }

    TEST_F(ServeTests, DisallowedDownloadReturnsMethodNotAllowed)
    {
        const auto res = Curl::Request{}.get(url("/nothingAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }

    TEST_F(ServeTests, DisallowedUploadReturnsMethodNotAllowed)
    {
        const auto res = Curl::Request{}.source("bla").put(url("/nothingAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }

    TEST_F(ServeTests, DisallowedDeleteReturnsMethodNotAllowed)
    {
        const auto res = Curl::Request{}.delete_(url("/nothingAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }

    TEST_F(ServeTests, DisallowedDeleteNonEmptyDirectoryReturnsForbidden)
    {
        EXPECT_TRUE(std::filesystem::exists(listener_->pathSupplier() / "nonEmptyDir"));
        const auto res = Curl::Request{}.delete_(url("/deleteAllowedButNotDirectories/nonEmptyDir"));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(ServeTests, CanDeleteEmptyDirectory)
    {
        EXPECT_TRUE(std::filesystem::exists(listener_->pathSupplier() / "emptyDir"));
        const auto res = Curl::Request{}.delete_(url("/allAllowed/emptyDir"));
        EXPECT_EQ(res.code(), boost::beast::http::status::no_content);
        EXPECT_FALSE(std::filesystem::exists(listener_->pathSupplier() / "emptyDir"));
    }

    TEST_F(ServeTests, CanDeleteNonEmptyDirectoryWhenAllowed)
    {
        EXPECT_TRUE(std::filesystem::exists(listener_->pathSupplier() / "nonEmptyDir"));
        const auto res = Curl::Request{}.delete_(url("/allAllowed/nonEmptyDir"));
        EXPECT_EQ(res.code(), boost::beast::http::status::no_content);
        EXPECT_FALSE(std::filesystem::exists(listener_->pathSupplier() / "nonEmptyDir"));
    }

    TEST_F(ServeTests, UserDeniedRequestReturnsForbidden)
    {
        const auto res = Curl::Request{}.get(url("/deny/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(ServeTests, UserDeniedRequestWithHandledDoesWhatUserDoes)
    {
        const auto res = Curl::Request{}.get(url("/customDeny/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);
    }

    TEST_F(ServeTests, SecureServerOnlyAllowsSecureServes)
    {
        const auto res = Curl::Request{}.get(urlEncryptedServer("/1/file.txt", {.secure = false}));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(ServeTests, SecureServerAllowUnsecureServesWhenAllowed)
    {
        const auto res = Curl::Request{}.get(urlEncryptedServer("/allAllowed/file.txt", {.secure = false}));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
    }

    TEST_F(ServeTests, UnrelatedMethodReturnsNotFound)
    {
        const auto res = Curl::Request{}.patch(url("/1/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::not_found);
    }

    TEST_F(ServeTests, CanDownloadFile)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/allAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, ServingListener::DummyFileContent);
    }

    TEST_F(ServeTests, CannotDownloadFileIfItDoesNotExist)
    {
        const auto res = Curl::Request{}.get(url("/allAllowed/file_404.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::not_found);
    }

    TEST_F(ServeTests, CanDeleteFile)
    {
        EXPECT_TRUE(std::filesystem::exists(listener_->pathSupplier() / "file.txt"));
        const auto res = Curl::Request{}.delete_(url("/allAllowed/file.txt"));
        EXPECT_FALSE(std::filesystem::exists(listener_->pathSupplier() / "file.txt"));
    }

    TEST_F(ServeTests, CanGetDirectoryListingIfAllowed)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/allAllowed"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_TRUE(body.starts_with("<!DOCTYPE"));
    }

    TEST_F(ServeTests, WillGetIndexIfDirectoryListingIsNotAllowed)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/1"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_TRUE(body.starts_with(ServingListener::DummyFileContent));
    }

    TEST_F(ServeTests, DirectoryListingReturnsErrorIfNotAllowed)
    {
        const auto res = Curl::Request{}.get(url("/nothingAllowed/nonEmptyDir"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }

    TEST_F(ServeTests, CanUploadFile)
    {
        const auto res = Curl::Request{}.source("Yes I am there.").put(url("/allAllowed/emptyDir/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        std::ifstream reader{listener_->pathSupplier() / "emptyDir/file.txt", std::ios::binary};
        std::string body;
        std::getline(reader, body);
        EXPECT_EQ(body, "Yes I am there.");
    }

    TEST_F(ServeTests, CanOverwriteFileIfAllowed)
    {
        const auto res = Curl::Request{}.source("Yes I am there.").put(url("/allAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        std::ifstream reader{listener_->pathSupplier() / "file.txt", std::ios::binary};
        std::string body;
        std::getline(reader, body);
        EXPECT_EQ(body, "Yes I am there.");
    }

    TEST_F(ServeTests, CannotUploadWhereSomethingExistsThatIsNotARegularFile)
    {
        const auto res = Curl::Request{}.source("Yes I am there.").put(url("/allAllowed/emptyDir"));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(ServeTests, CannotUploadToExistingFileWhenDisallowed)
    {
        const auto res = Curl::Request{}.source("Yes I am there.").put(url("/overwriteNotAllowed/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(ServeTests, CannotUploadFileWithout100ContinueHandling)
    {
        const auto res = Curl::Request{}
                             .source("Yes I am there.")
                             .setHeaderField("Expect", "")
                             .put(url("/allAllowed/emptyDir/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::expectation_failed);
    }

    // Test is not exhaustive.
    TEST_F(ServeTests, CannotAccessFileOutsideOfJail)
    {
        const auto res =
            Curl::Request{}.source("Yes I am there.").setHeaderField("Expect", "").put(url("/deep/../../../file.txt"));
        // There is a file there, but should report as not_found.
        EXPECT_EQ(res.code(), boost::beast::http::status::not_found);
    }

    TEST_F(ServeTests, CanModifyPermissionsOnTheFly)
    {
        std::string body;
        const auto res = Curl::Request{}.sink(body).get(url("/nothingIsAllowedButIsOverruled/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, ServingListener::DummyFileContent);
    }

    TEST_F(ServeTests, UnmodifiedDisallowedPermissionsAreStillDisallowed)
    {
        const auto res = Curl::Request{}.source("test").put(url("/nothingIsAllowedButIsOverruled/file.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::method_not_allowed);
    }
}