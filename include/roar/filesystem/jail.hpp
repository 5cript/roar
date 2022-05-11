#pragma once

#include <filesystem>
#include <optional>

namespace Roar
{
    /**
     *  Not meant to be 100% abuse proof.
     */
    class Jail
    {
      public:
        /**
         * @brief Construct a new Jail object that can be used to restrict paths to "jailRoot".
         *
         * @param jailRoot The path to restrict relative paths to.
         */
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

        /**
         * @brief First tests if the path is in the jail and then returns a full path of the resource including the jail
         * path.
         *
         * @param other A relative path to the jail.
         * @return std::optional<std::filesystem::path> Path or nothing if not in jail.
         */
        std::optional<std::filesystem::path> pathAsIsInJail(std::filesystem::path const& other) const;

        /**
         * @brief Returns the jailed path as if it were coming from root like "/jail/asdf.txt".
         *
         * @param other A relative path to the jail.
         * @return std::filesystem::path Path or nothing if not in jail.
         */
        std::filesystem::path fakeJailAsRoot(std::filesystem::path const& other) const;

      private:
        std::filesystem::path jailRoot_;
    };
}