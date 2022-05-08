#pragma once

#include <filesystem>
#include <optional>

namespace Roar::Detail
{
    /**
     *  Not meant to be 100% abuse proof.
     */
    class Jail
    {
      public:
        Jail(std::filesystem::path const& jailRoot);

        /**
         *  Is the given path within the jail.
         */
        bool isWithinJail(std::filesystem::path const& other) const;

        /**
         *  Will return a path, that is relative to the jail, if the path is within the jail.
         */
        std::optional<std::filesystem::path>
        relativeToRoot(std::filesystem::path const& other, bool fakeJailAsRoot = false) const;

        std::optional<std::filesystem::path> pathAsIsInJail(std::filesystem::path const& other) const;

        std::filesystem::path fakeJailAsRoot(std::filesystem::path const& other) const;

      private:
        std::filesystem::path jailRoot_;
    };
}