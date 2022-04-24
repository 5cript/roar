#pragma once

#include "util/common_server_setup.hpp"

#include <roar/routing/request_listener.hpp>

#include <gtest/gtest.h>

namespace Roar::Tests
{
    // class RequestListener
    // {
    //   private:
    //     ROAR_MAKE_LISTENER(RequestListener);

    //     ROAR_GET(withCors)
    //     ({
    //         .path = "/cors",
    //         .routeOptions =
    //             {
    //                 .allowCors = true,
    //             },
    //     });
    //     ROAR_GET(noCors)
    //     ({
    //         .path = "/noCors",
    //         .routeOptions =
    //             {
    //                 .allowCors = false,
    //             },
    //     });
    //     ROAR_GET(noCorsByDefault)
    //     ({
    //         .path = "/noCorsByDefault",
    //     });

    //   private:
    //     BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_withCors, roar_noCors, roar_noCorsByDefault))
    // };

    // class CorsTests
    //     : public CommonServerSetup
    //     , public ::testing::Test
    // {
    //   protected:
    //     void SetUp() override
    //     {
    //         makeDefaultServer();
    //     }
    // };

    // TEST(CorsTests, CorsIsDisabledByDefault)
    // {}
}