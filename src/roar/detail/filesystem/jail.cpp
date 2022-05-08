#include <roar/detail/filesystem/jail.hpp>

#include <string>

using namespace std::string_literals;
//#####################################################################################################################
/*  root-name: the longest valid sequence of characters that represent a root name are the root name. (C: or //server)
 *  file-name: a block between directory seperators. | dot '.' and dot dot '..' are special file-names.
 *  directory-seperator: unless its the first, a repeated directory seperator is treated the same as a single one:
 *      /a///b is the same as /a/b
 */
//#####################################################################################################################
namespace Roar::Detail
{
    //#####################################################################################################################
    Jail::Jail(std::filesystem::path const& jailRoot)
        : jailRoot_{std::filesystem::canonical(jailRoot).generic_string()}
    {}
    //---------------------------------------------------------------------------------------------------------------------
    bool Jail::isWithinJail(std::filesystem::path const& other) const
    {
        // potentially does more work than necessary.
        return relativeToRoot(other) != std::nullopt;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::optional<std::filesystem::path>
    Jail::relativeToRoot(std::filesystem::path const& other, bool fakeJailAsRoot) const
    {
        std::error_code ec;
        auto proxi = std::filesystem::proximate(other, jailRoot_, ec);
        if (ec)
            return std::nullopt;
        for (auto const& part : proxi)
        {
            if (part == "..")
                return std::nullopt;
        }
        if (fakeJailAsRoot)
            return this->fakeJailAsRoot(proxi);
        else
            return {proxi};
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::filesystem::path Jail::fakeJailAsRoot(std::filesystem::path const& other) const
    {
        return std::filesystem::path{"/"s + jailRoot_.filename().string() + "/" + other.generic_string()};
    }
    //#####################################################################################################################
}