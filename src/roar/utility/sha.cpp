#include <roar/utility/sha.hpp>

#include <openssl/sha.h>

#include <sstream>
#include <memory>
#include <functional>
#include <iomanip>

namespace Roar
{
    //#####################################################################################################################
    namespace
    {
        std::string bufToHex(unsigned char* buffer, std::size_t length)
        {
            std::stringstream ss;
            for (unsigned int i = 0; i != length; ++i)
            {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
            }
            return ss.str();
        }
    }
    //#####################################################################################################################
    std::optional<std::string> sha256(std::string_view data)
    {
        SHA256_CTX ctx;
        unsigned char buffer[SHA256_DIGEST_LENGTH]{0};

        if (!SHA256_Init(&ctx))
            return std::nullopt;

        if (!SHA256_Update(&ctx, data.data(), data.size()))
            return std::nullopt;

        if (!SHA256_Final(buffer, &ctx))
            return std::nullopt;

        return bufToHex(buffer, SHA256_DIGEST_LENGTH);
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::optional<std::string> sha512(std::string_view data)
    {
        SHA512_CTX ctx;
        unsigned char buffer[SHA512_DIGEST_LENGTH]{0};

        if (!SHA512_Init(&ctx))
            return std::nullopt;

        if (!SHA512_Update(&ctx, data.data(), data.size()))
            return std::nullopt;

        if (!SHA512_Final(buffer, &ctx))
            return std::nullopt;

        return bufToHex(buffer, SHA512_DIGEST_LENGTH);
    }
    //#####################################################################################################################
}