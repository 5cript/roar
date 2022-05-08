#pragma once

#include "util/common_listeners.hpp"
#include "util/common_server_setup.hpp"

#include <gtest/gtest.h>

namespace Roar::Tests
{
    class ServingListener
    {
      public:
        std::string body;
        std::size_t contentLength;

      private:
        ROAR_MAKE_LISTENER(ServingListener);

        ROAR_SERVE(serve)("/", "~");

      private:
        BOOST_DESCRIBE_CLASS(ServingListener, (), (), (), (roar_serve))
    };

    inline Roar::ServeDecision ServingListener::serve(
        Roar::Session& session,
        Roar::EmptyBodyRequest const& request,
        Roar::FileAndStatus const& fileAndStatus)
    {
        return Roar::ServeDecision::Deny;
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
            listener_ = server_->installRequestListener<ServingListener>();
        }

      protected:
        std::shared_ptr<ServingListener> listener_;
    };

    TEST_F(ServeTests, CanMakeHeadRequest)
    {
        // TODO:
    }

    TEST_F(ServeTests, OptionsRequestReturnsPermittedMethods)
    {
        // TODO:
    }

    TEST_F(ServeTests, DisallowedDownloadReturnsMethodNotAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, DisallowedUploadReturnsMethodNotAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, DisallowedDeleteReturnsMethodNotAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, DisallowedDeleteNonEmptyDirectoryReturnsForbidden)
    {
        // TODO:
    }

    TEST_F(ServeTests, UserDeniedRequestReturnsForbidden)
    {
        // TODO:
    }

    TEST_F(ServeTests, UserDeniedRequestWithJustCloseDoesWhatUserDoes)
    {
        // TODO:
    }

    TEST_F(ServeTests, SecureServerOnlyAllowsSecureServes)
    {
        // TODO:
    }

    TEST_F(ServeTests, SecureServerAllowUnsecureServesWhenAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, UnrelatedMethodReturnsNotFound)
    {
        // TODO:
    }

    TEST_F(ServeTests, CanDownloadFile)
    {
        // TODO:
    }

    TEST_F(ServeTests, CannoDownloadFileIfItDoesNotExist)
    {
        // TODO:
    }

    TEST_F(ServeTests, CanDownloadFileRange)
    {
        // TODO:
    }

    TEST_F(ServeTests, CanDeleteFile)
    {
        // TODO:
    }

    TEST_F(ServeTests, CanGetDirectoryListingIfAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, DirectoryListingReturnsErrorIfNotAllowed)
    {
        // TODO:
    }

    TEST_F(ServeTests, CanUploadFile)
    {
        // TODO:
    }

    TEST_F(ServeTests, CannotUploadFileWithout100ContinueHandling)
    {
        // TODO:
    }

    TEST_F(ServeTests, CannotAccessFileOutsideOfJail)
    {
        // TODO:
    }
}