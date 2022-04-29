#include <roar/curl/sources/string_source.hpp>

#include <algorithm>
#include <iterator>
#include <utility>
#include <ios>

namespace Roar::Curl
{
    StringSource::StringSource(std::string str)
        : data_{std::move(str)}
        , offset_{0}
    {}
    std::size_t StringSource::fetch(char* buffer, std::size_t amount)
    {
        if (offset_ >= data_.size())
            return 0;

        amount = std::min(amount, data_.size() - offset_);
        std::copy(
            std::begin(data_) + static_cast<std::streamsize>(offset_),
            std::begin(data_) + static_cast<std::streamsize>(offset_ + amount),
            buffer);
        offset_ += amount;
        return amount;
    }
    std::size_t StringSource::size()
    {
        return data_.size();
    }
}
