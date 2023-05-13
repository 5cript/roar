#include <roar/utility/sha.hpp>

#include <openssl/evp.h>

#include <sstream>
#include <memory>
#include <functional>
#include <iomanip>

namespace Roar
{
    // #####################################################################################################################
    namespace
    {
        std::string bufToHex(unsigned char* buffer, std::size_t length)
        {
            std::stringstream ss;
            for (unsigned int i = 0; i != length; ++i)
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
            return ss.str();
        }

        std::optional<std::string> digest(std::string_view data, auto digestAlgorithm)
        {
            std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX*)> digestContext{EVP_MD_CTX_new(), [](EVP_MD_CTX* ctx) {
                                                                                 EVP_MD_CTX_free(ctx);
                                                                             }};

            unsigned char digestBuffer[EVP_MAX_MD_SIZE];
            unsigned int digestLength = EVP_MAX_MD_SIZE;

            if (!EVP_DigestInit_ex(digestContext.get(), digestAlgorithm(), nullptr))
                return std::nullopt;

            if (!EVP_DigestUpdate(digestContext.get(), data.data(), data.size()))
                return std::nullopt;

            if (!EVP_DigestFinal_ex(digestContext.get(), digestBuffer, &digestLength))
                return std::nullopt;

            return bufToHex(digestBuffer, digestLength);
        }
    }
    // #####################################################################################################################
    std::optional<std::string> sha256(std::string_view data)
    {
        return digest(data, EVP_sha256);
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::optional<std::string> sha512(std::string_view data)
    {
        return digest(data, EVP_sha512);
    }
    // #####################################################################################################################
}