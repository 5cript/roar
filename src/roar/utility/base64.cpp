#include <roar/utility/base64.hpp>

#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/detail/string.hpp>

#include <algorithm>

using namespace boost::beast::detail;

namespace Roar
{
    namespace
    {
        char const* base64UlrAlphabet()
        {
            static char constexpr tab[] = {
                "ABCDEFGHIJKLMNOP"
                "QRSTUVWXYZabcdef"
                "ghijklmnopqrstuv"
                "wxyz0123456789-_"};
            return &tab[0];
        }

        /// Slight base64url modification from boost beasts base64 inverse table.
        signed char const* base64UrlAlphabetInverse()
        {
            static signed char constexpr tab[] = {
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //   0-15
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //  16-31
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, //  32-47
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, //  48-63
                -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, //  64-79
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, //  80-95
                -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //  96-111
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // 112-127
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 128-143
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 144-159
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 160-175
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 176-191
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 192-207
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 208-223
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 224-239
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 // 240-255
            };
            return &tab[0];
        }

        std::size_t base64UrlEncodeImpl(void* dest, void const* src, std::size_t len, std::size_t& padding)
        {
            char* out = static_cast<char*>(dest);
            char const* in = static_cast<char const*>(src);
            auto const tab = base64UlrAlphabet();

            for (auto n = len / 3; n--;)
            {
                *out++ = tab[(in[0] & 0xfc) >> 2];
                *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
                *out++ = tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
                *out++ = tab[in[2] & 0x3f];
                in += 3;
            }

            switch (len % 3)
            {
                case 2:
                    *out++ = tab[(in[0] & 0xfc) >> 2];
                    *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
                    *out++ = tab[(in[1] & 0x0f) << 2];
                    padding = 1;
                    break;

                case 1:
                    *out++ = tab[(in[0] & 0xfc) >> 2];
                    *out++ = tab[((in[0] & 0x03) << 4)];
                    padding = 2;
                    break;

                case 0:
                    break;
            }

            return static_cast<std::size_t>(out - static_cast<char*>(dest));
        }

        std::pair<std::size_t, std::size_t> base64UrlDecodeImpl(void* dest, char const* src, std::size_t len)
        {
            char* out = static_cast<char*>(dest);
            auto in = reinterpret_cast<unsigned char const*>(src);
            unsigned char c3[3], c4[4];
            int i = 0;
            int j = 0;

            auto const inverse = base64UrlAlphabetInverse();

            while (len-- && *in != '=' && *in != '%')
            {
                auto const v = inverse[*in];
                if (v == -1)
                    break;
                ++in;
                c4[i] = static_cast<unsigned char>(v);
                if (++i == 4)
                {
                    c3[0] = static_cast<unsigned char>((c4[0] << 2) + ((c4[1] & 0x30) >> 4));
                    c3[1] = static_cast<unsigned char>(((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2));
                    c3[2] = static_cast<unsigned char>(((c4[2] & 0x3) << 6) + c4[3]);

                    for (i = 0; i < 3; i++)
                        *out++ = static_cast<char>(c3[i]);
                    i = 0;
                }
            }

            if (i)
            {
                c3[0] = static_cast<unsigned char>((c4[0] << 2) + ((c4[1] & 0x30) >> 4));
                c3[1] = static_cast<unsigned char>(((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2));
                c3[2] = static_cast<unsigned char>(((c4[2] & 0x3) << 6) + c4[3]);

                for (j = 0; j < i - 1; j++)
                    *out++ = static_cast<char>(c3[j]);
            }

            return {out - static_cast<char*>(dest), in - reinterpret_cast<unsigned char const*>(src)};
        }

    } // namespace

    std::string base64Encode(std::vector<unsigned char> const& data)
    {
        std::string result(base64::encoded_size(data.size()), '\0');
        const auto size = base64::encode(result.data(), data.data(), data.size());
        result.resize(size);
        return result;
    }

    std::string base64Encode(std::string const& str)
    {
        std::string result(base64::encoded_size(str.size()), '\0');
        const auto size = base64::encode(result.data(), str.c_str(), str.size());
        result.resize(size);
        return result;
    }
    std::string base64Decode(std::string const& base64String)
    {
        return base64Decode(std::string_view{base64String});
    }
    std::string base64Decode(std::string_view base64View)
    {
        auto decodedSize = base64::decoded_size(base64View.size());
        if (decodedSize == 0)
            return {};
        std::string result(decodedSize, '\0');
        const auto [writtenOut, readIn] = base64::decode(result.data(), base64View.data(), base64View.size());
        result.resize(writtenOut);
        return result;
    }
    std::string base64UrlEncode(std::vector<unsigned char> const& data, bool includePadding)
    {
        std::string result(base64::encoded_size(data.size()), '\0');
        std::size_t padding = 0;
        base64UrlEncodeImpl(result.data(), data.data(), data.size(), padding);
        if (includePadding)
        {
            const auto offset = result.size() - padding;
            if (padding > 0)
                result.resize(result.size() + padding * 3 - padding);
            for (std::size_t i = 0; i != padding; ++i)
            {
                result[offset + i * 3] = '%';
                result[offset + i * 3 + 1] = '3';
                result[offset + i * 3 + 2] = 'D';
            }
        }
        else if (padding > 0)
        {
            result.resize(result.size() - padding);
        }
        return result;
    }
    std::vector<unsigned char> base64UrlDecode(std::string_view const& base64View)
    {
        // Adding some additional pessimistic size, since the padding might be missing resulting in a too
        // short vector.
        std::vector<unsigned char> result(base64::decoded_size(base64View.size()) + 3);
        const auto [writtenOut, readIn] = base64UrlDecodeImpl(result.data(), base64View.data(), base64View.size());
        result.resize(writtenOut);
        return result;
    }
}
