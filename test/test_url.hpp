#pragma once

#include <roar/url/url.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>

namespace Roar::Tests
{
    class UrlTests : public ::testing::Test
    {
      public:
        void testAddress(std::string const& addr, std::optional<std::string> testAgainst = std::nullopt) const
        {
            if (!testAgainst)
                testAgainst = addr;

            using namespace std::string_literals;
            auto url = Url::fromString("https://"s + addr + ":7070/");
            ASSERT_TRUE(url);
            EXPECT_EQ(url.value().hostAsString(), *testAgainst);
        }

        void testAddressFail(std::string const& addr) const
        {
            using namespace std::string_literals;
            const auto url = Url::fromString("https://"s + addr + ":7070/");
            EXPECT_FALSE(url) << url.value().toString();
        }

        bool hostIsDomain(boost::leaf::result<Url> const& url) const
        {
            return url && std::holds_alternative<std::string>(url.value().authority.remote.host);
        }
    };

    TEST_F(UrlTests, CanConvertMostSimpleUrlToString)
    {
        Url uri{.scheme = "A", .authority = {.remote = {.host = "HOST"}}};
        EXPECT_EQ(uri.toString(), "A://HOST");
    }

    TEST_F(UrlTests, CanConvertUrlWithPortToString)
    {
        Url uri{.scheme = "A", .authority = {.remote = {.host = "HOST", .port = 84}}};
        EXPECT_EQ(uri.toString(), "A://HOST:84");
    }

    TEST_F(UrlTests, CanConvertUrlWithAuthentication)
    {
        Url uri{
            .scheme = "A",
            .authority =
                {
                    .userInfo = {{
                        .user = "user",
                        .password = "passw",
                    }},
                    .remote = {.host = "HOST", .port = 84},
                },
        };
        EXPECT_EQ(uri.toString(), "A://user:passw@HOST:84");
    }

    TEST_F(UrlTests, CanConvertUrlWithAuthUserOnly)
    {
        Url uri{
            .scheme = "A",
            .authority = {.userInfo = {{.user = "user"}}, .remote = {.host = "HOST", .port = 84}},
        };
        EXPECT_EQ(uri.toString(), "A://user@HOST:84");
    }

    TEST_F(UrlTests, CanConvertUrlWithPath)
    {
        Url uri{
            .scheme = "A",
            .authority =
                {
                    .remote = {.host = "HOST", .port = 84},
                },
            .path = {"path", "asdf"},
        };
        EXPECT_EQ(uri.toString(), "A://HOST:84/path/asdf");
    }

    TEST_F(UrlTests, CanConvertUrlWithQuery)
    {
        Url uri{
            .scheme = "A",
            .authority =
                {
                    .remote = {.host = "HOST", .port = 84},
                },
            .path = {"path", "asdf"},
            .query = {{"key", "value"}, {"key1", "value1"}},
        };
        const std::string base = "A://HOST:84/path/asdf";
        EXPECT_TRUE(
            uri.toString() == (base + "?key=value&key1=value1") || uri.toString() == (base + "?key1=value1&key=value"));
    }

    TEST_F(UrlTests, CanConvertUrlWithAllCapabilities)
    {
        Url uri{
            .scheme = "https",
            .authority =
                {
                    .userInfo = {{
                        .user = "lynx",
                        .password = "paw",
                    }},
                    .remote = {.host = "[::1]", .port = 729},
                },
            .path = {"api", "v0", "x"},
            .query = {{"token", "0"}, {"sort", "true"}},
        };
        const std::string base = "https://lynx:paw@[::1]:729/api/v0/x";
        EXPECT_TRUE(uri.toString() == (base + "?token=0&sort=true") || uri.toString() == (base + "?sort=true&token=0"));
    }

    TEST_F(UrlTests, CanConvertUrlAuthorityOnly)
    {
        Url uri{
            .scheme = "https",
            .authority =
                {
                    .userInfo = {{
                        .user = "lynx",
                        .password = "paw",
                    }},
                    .remote = {.host = "[::1]", .port = 729},
                },
            .path = {"api", "v0", "x"},
            .query = {{"token", "0"}, {"sort", "true"}},
        };
        EXPECT_EQ(uri.schemeAndAuthority(), "https://lynx:paw@[::1]:729");
    }

    TEST_F(UrlTests, CanParseDifferentSchemes)
    {
        auto testScheme = [](bool forSuccess, std::string const& scheme) {
            auto res = Url::fromString(scheme + "://bla.com");
            if (forSuccess)
            {
                ASSERT_TRUE(res) << scheme << " ";
                EXPECT_EQ(res.value().scheme, scheme);
            }
            else
            {
                ASSERT_FALSE(res);
            }
        };
        testScheme(true, "https");
        testScheme(true, "http");
        testScheme(true, "http");
        testScheme(true, "h0.");
        testScheme(true, "s+ae-a");
        testScheme(true, "H+.-");

        // real examples:
        testScheme(true, "com-eventbrite-attendee");
        testScheme(true, "iris.xpcs");
        testScheme(true, "web+...");
        testScheme(true, "z39.50r");

        testScheme(false, "+x");
        testScheme(false, "-x");
        testScheme(false, ".x");
        testScheme(false, "0x");
        testScheme(false, "ü");
        testScheme(false, "");
    }

    TEST_F(UrlTests, CanParseIpv6Urls)
    {
        testAddress("[1:2:3:4:5:6:7:8]", "[0001:0002:0003:0004:0005:0006:0007:0008]");
        testAddress("[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
        testAddress("[0000:0000:0000:0000:0000:0000:0000:0000]");
        testAddress("[1:2:3:4:5:6:7::]", "[0001:0002:0003:0004:0005:0006:0007:0000]");
        testAddress("[1:2:3:4:5:6::]", "[0001:0002:0003:0004:0005:0006:0000:0000]");
        testAddress("[1:2:3:4:5:6:192.168.1.0]", "[0001:0002:0003:0004:0005:0006:192.168.1.0]");
        testAddress("[1:2:3:4:5::]", "[0001:0002:0003:0004:0005:0000:0000:0000]");
        testAddress("[1:2:3:4:5::192.168.1.0]", "[0001:0002:0003:0004:0005:0000:192.168.1.0]");
        testAddress("[1:2:3:4::]", "[0001:0002:0003:0004:0000:0000:0000:0000]");
        testAddress("[1:2:3:4::192.168.1.0]", "[0001:0002:0003:0004:0000:0000:192.168.1.0]");
        testAddress("[1:2:3::]", "[0001:0002:0003:0000:0000:0000:0000:0000]");
        testAddress("[1:2:3::192.168.1.0]", "[0001:0002:0003:0000:0000:0000:192.168.1.0]");
        testAddress("[1:2::]", "[0001:0002:0000:0000:0000:0000:0000:0000]");
        testAddress("[1:2::192.168.1.0]", "[0001:0002:0000:0000:0000:0000:192.168.1.0]");
        testAddress("[1::]", "[0001:0000:0000:0000:0000:0000:0000:0000]");
        testAddress("[1::192.168.1.0]", "[0001:0000:0000:0000:0000:0000:192.168.1.0]");
        testAddress("[::]", "[0000:0000:0000:0000:0000:0000:0000:0000]");
        testAddress("[::192.168.1.0]", "[0000:0000:0000:0000:0000:0000:192.168.1.0]");
        testAddress("[::1]", "[0000:0000:0000:0000:0000:0000:0000:0001]");
        testAddress("[::1:192.168.1.0]", "[0000:0000:0000:0000:0000:0001:192.168.1.0]");
        testAddress("[::1:2]", "[0000:0000:0000:0000:0000:0000:0001:0002]");
        testAddress("[::1:2:192.168.1.0]", "[0000:0000:0000:0000:0001:0002:192.168.1.0]");
        testAddress("[::1:2:3]", "[0000:0000:0000:0000:0000:0001:0002:0003]");
        testAddress("[::1:2:3:192.168.1.0]", "[0000:0000:0000:0001:0002:0003:192.168.1.0]");
        testAddress("[::1:2:3:4]", "[0000:0000:0000:0000:0001:0002:0003:0004]");
        testAddress("[::1:2:3:4:192.168.1.0]", "[0000:0000:0001:0002:0003:0004:192.168.1.0]");
        testAddress("[::1:2:3:4:5]", "[0000:0000:0000:0001:0002:0003:0004:0005]");
        testAddress("[::1:2:3:4:5:192.168.1.0]", "[0000:0001:0002:0003:0004:0005:192.168.1.0]");
        testAddress("[::1:2:3:4:5:6]", "[0000:0000:0001:0002:0003:0004:0005:0006]");
        testAddress("[::1:2:3:4:5:6:7]", "[0000:0001:0002:0003:0004:0005:0006:0007]");

        testAddress("[1::4]", "[0001:0000:0000:0000:0000:0000:0000:0004]");
        testAddress("[1::4:5:6:7]", "[0001:0000:0000:0000:0004:0005:0006:0007]");
        testAddress("[1:2::4:5:6:7]", "[0001:0002:0000:0000:0004:0005:0006:0007]");
        testAddress("[1:2:3::5:6:7]", "[0001:0002:0003:0000:0000:0005:0006:0007]");
        testAddress("[1:2:3:4::6:7]", "[0001:0002:0003:0004:0000:0000:0006:0007]");
        testAddress("[1:2:3:4:5::7]", "[0001:0002:0003:0004:0005:0000:0000:0007]");
    }

    TEST_F(UrlTests, LocalhostWithPortIsParsed)
    {
        auto maybeUrl = Url::fromString("http://localhost:53879/");
        ASSERT_TRUE(maybeUrl);
    }

    TEST_F(UrlTests, InvalidIpv6AddressesMakeTheParseFail)
    {
        testAddressFail("[]");
        testAddressFail("[1:]");
        testAddressFail("[1:2]");
        testAddressFail("[1:2:3:4:5:6:7:8:9]");
        testAddressFail("[::1:2:3:4:5:6:7:8]");
        testAddressFail("[1:2:3:4:5:6:7:8::]");
        testAddressFail("[1:2:3:4:5:6:7:128.21.32.1]");
        testAddressFail("[1:2:3:4:5:6:384.21.32.1]");
    }

    TEST_F(UrlTests, CanParseIpv4Url)
    {
        testAddress("192.168.0.1");
        testAddress("255.255.255.255");
        testAddress("0.0.0.0");
    }

    // Unfortunately these invalid ipv4 addresses are actually valid hostnames.
    TEST_F(UrlTests, InvalidIpv4AddressesAreInterpretedAsDomain)
    {
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://384.168.0.1")));
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://255.384.255.255")));
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://0.0.364.0")));
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://0.0.0.6848")));
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://0.0.x.0")));
        EXPECT_TRUE(hostIsDomain(Url::fromString("https://0.1.0")));
    }

    TEST_F(UrlTests, PathIsCorrectlyParsedOffOfTheUrl)
    {
        auto maybeUrl = Url::fromString("https://does.not.exist.com/");
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().pathAsString(), "/");

        maybeUrl = Url::fromString("https://does.not.exist.com/first/second");
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().pathAsString(), "/first/second");
    }

    TEST_F(UrlTests, PathMayContainPercentEncodedCharacters)
    {
        auto maybeUrl = Url::fromString("https://does.not.exist.com/as%20df");
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().pathAsString(), "/as%20df");
        EXPECT_EQ(maybeUrl.value().pathAsString(false), "/as df");
    }

    TEST_F(UrlTests, QueryIsParsedOffOfTheUrl)
    {
        const auto maybeUrl = Url::fromString("https://does.not.exist.com/bla?as%3Fdf=;key=value");
        ASSERT_TRUE(maybeUrl);
        auto url = maybeUrl.value();
        ASSERT_EQ(url.query["as?df"], "");
        ASSERT_EQ(url.query["key"], "value");
    }

    TEST_F(UrlTests, FragmentIsParsedOffOfTheUrl)
    {
        auto maybeUrl = Url::fromString("https://does.not.exist.com/bla#frag");
        ASSERT_TRUE(maybeUrl);
        auto url = maybeUrl.value();
        ASSERT_EQ(url.fragment, "frag");

        maybeUrl = Url::fromString("https://does.not.exist.com/bla?k=v#fragment");
        ASSERT_TRUE(maybeUrl);
        url = maybeUrl.value();
        ASSERT_EQ(url.fragment, "fragment");
    }

    TEST_F(UrlTests, PercentEncodingIsNotAllowedInDomainName)
    {
        std::string urlString = "mailto://test%20this%20out%7D:12345/";
        auto maybeUrl = Url::fromString(urlString);
        ASSERT_FALSE(maybeUrl);
    }

    TEST_F(UrlTests, CredentialsAreParsedOffOfTheUrl)
    {
        // !%22%C2%A7%24
        std::string urlString = "https://us%3fer%3a:!%22%C2%A7%24@blabla.net";
        auto maybeUrl = Url::fromString(urlString);
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().authority.userInfo->user, "us?er:");
        ASSERT_TRUE(maybeUrl.value().authority.userInfo->password);
        EXPECT_EQ(*maybeUrl.value().authority.userInfo->password, "!\"§$");
    }

    TEST_F(UrlTests, CredentialPasswordCanBeEmpty)
    {
        std::string urlString = "https://admin:@blabla.net";
        auto maybeUrl = Url::fromString(urlString);
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().authority.userInfo->user, "admin");
        ASSERT_TRUE(maybeUrl.value().authority.userInfo->password);
        EXPECT_EQ(maybeUrl.value().authority.userInfo->password, "");
    }

    TEST_F(UrlTests, CanParseUrlWithUserOnly)
    {
        std::string urlString = "https://admin@localhost";
        auto maybeUrl = Url::fromString(urlString);
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().authority.userInfo->user, "admin");
        ASSERT_FALSE(maybeUrl.value().authority.userInfo->password);
    }

    TEST_F(UrlTests, SynthesizedUrlStringIsTheSameAsTheParsedString)
    {
        std::string urlString =
            "mailto://us%3Fer:%21%22%C2%A7%24@blabla.net:12345/weird%20path%21here?ke%C2=%3D%26vla#Bla";
        auto maybeUrl = Url::fromString(urlString);
        ASSERT_TRUE(maybeUrl);
        EXPECT_EQ(maybeUrl.value().toString(true), urlString);
    }

    TEST_F(UrlTests, CanParsePath)
    {
        using namespace ::testing;
        auto maybeUrl = Url::parsePath("/path/to/resource");
        ASSERT_TRUE(maybeUrl);
        ASSERT_THAT(maybeUrl.value(), ElementsAre("path", "to", "resource"));
    }

    TEST_F(UrlTests, CanParsePathWithPercentEncodedCharacters)
    {
        using namespace ::testing;
        auto maybeUrl = Url::parsePath("/pa%20th/%3fto/resource");
        ASSERT_TRUE(maybeUrl);
        ASSERT_THAT(maybeUrl.value(), ElementsAre("pa th", "?to", "resource"));
    }

    TEST_F(UrlTests, PathThatIsEmptyYieldsEmptyContainer)
    {
        using namespace ::testing;
        auto maybeUrl = Url::parsePath("");
        ASSERT_TRUE(maybeUrl);
        ASSERT_TRUE(maybeUrl.value().empty());
    }

    TEST_F(UrlTests, PathThatIsJustASlashYieldsEmptyContainer)
    {
        using namespace ::testing;
        auto maybeUrl = Url::parsePath("/");
        ASSERT_TRUE(maybeUrl);
        ASSERT_TRUE(maybeUrl.value().empty());
    }

    TEST_F(UrlTests, PathNotStartingWithSlashIsInvalid)
    {
        using namespace ::testing;
        auto maybeUrl = Url::parsePath("bla");
        ASSERT_FALSE(maybeUrl);
    }

    TEST_F(UrlTests, AuthorityMayBeOmitted)
    {
        const auto url = Url::fromString("http:/path/here");
        ASSERT_TRUE(url);
        ASSERT_EQ(url.value().pathAsString(), "/path/here");
    }
}