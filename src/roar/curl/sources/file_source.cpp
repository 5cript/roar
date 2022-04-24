#include <roar/curl/sources/file_source.hpp>

#include <stdexcept>

namespace Roar::Curl
{
    FileSource::FileSource(std::filesystem::path const& filename)
        : reader_{filename, std::ios_base::binary}
    {
        if (!reader_.good())
            throw std::runtime_error("could not open file source.");
    }
    std::size_t FileSource::fetch(char* buffer, std::size_t amount)
    {
        reader_.read(buffer, static_cast<std::streamsize>(amount));
        return static_cast<std::size_t>(reader_.gcount());
    }
    std::size_t FileSource::size()
    {
        if (memoizedFileSize_)
            return *memoizedFileSize_;

        const auto savedPos = reader_.tellg();
        reader_.seekg(0, std::ios_base::end);
        memoizedFileSize_ = reader_.tellg();
        reader_.seekg(savedPos);

        return *memoizedFileSize_;
    }
}
