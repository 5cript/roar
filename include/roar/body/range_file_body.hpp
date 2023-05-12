#pragma once

#include <roar/mechanics/ranges.hpp>
#include <boost/beast/http/message.hpp>
#include <roar/literals/memory.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <random>
#include <string_view>
#include <algorithm>
#include <fmt/format.h>

namespace Roar
{
    namespace Detail
    {
        /**
         * @brief Support range requests for get requests including multipart/byterange.
         */
        class RangeFileBodyImpl
        {
            constexpr static unsigned SplitterLength = 16;

          private:
            struct Sequence
            {

                Sequence() = default;
                Sequence(
                    std::uint64_t start,
                    std::uint64_t end,
                    std::uint64_t totalFile,
                    std::string_view contentType,
                    std::string const& splitter)
                    : start{start}
                    , end{end}
                    , headConsumed{0}
                    , headSection{fmt::format(
                          "\r\n{}\r\n"
                          "Content-Type: {}\r\n"
                          "Content-Range: bytes {}-{}/{}\r\n\r\n",
                          splitter,
                          contentType,
                          start,
                          end,
                          totalFile)}
                {}
                Sequence(std::string const& splitter)
                    : start{0}
                    , end{0}
                    , headConsumed{0}
                    , headSection{std::string{"\r\n"} + splitter}
                {}
                Sequence(std::uint64_t start, std::uint64_t end)
                    : start{start}
                    , end{end}
                    , headConsumed{0}
                    , headSection{}
                {}
                std::uint64_t start;
                std::uint64_t end;
                std::uint64_t headConsumed;
                std::string headSection;
            };

          public:
            RangeFileBodyImpl()
                : file_{}
                , sequences_{}
                , splitter_(SplitterLength + 2, '-')
                , totalSize_{0}
                , consumed_{0}
            {
                constexpr const char* hexCharacters = "0123456789ABCDEF";

                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> distrib(0, 15);
                std::generate_n(std::next(std::next(std::begin(splitter_))), SplitterLength, [&]() {
                    return static_cast<char>(hexCharacters[distrib(gen)]);
                });
            }
            ~RangeFileBodyImpl() = default;
            RangeFileBodyImpl(RangeFileBodyImpl const&) = delete;
            RangeFileBodyImpl(RangeFileBodyImpl&&) = default;
            RangeFileBodyImpl& operator=(RangeFileBodyImpl const&) = delete;
            RangeFileBodyImpl& operator=(RangeFileBodyImpl&&) = default;

            std::fstream& file()
            {
                return file_;
            }

            std::uint64_t size() const
            {
                return totalSize_;
            }

            std::uint64_t remaining() const
            {
                return totalSize_ - consumed_;
            }

            bool isMultipart() const
            {
                return sequences_.size() > 1;
            }

            std::pair<std::uint64_t, std::uint64_t> firstRange()
            {
                return {sequences_.rbegin()->start, sequences_.rbegin()->end};
            }

            std::uint64_t fileSize()
            {
                auto pos = file_.tellg();
                file_.seekg(0, std::ios::end);
                auto size = file_.tellg();
                file_.seekg(pos);
                return static_cast<std::uint64_t>(size);
            }

            bool isOpen() const
            {
                return file_.is_open();
            }

            void close()
            {
                file_.close();
            }

            /**
             * @brief Opens the file.
             *
             * @param filename
             * @param mode
             * @param ec
             */
            void open(std::filesystem::path const& filename, std::ios_base::openmode mode, std::error_code& ec)
            {
                ec = {};
                auto excMask = file_.exceptions();
                file_.exceptions(excMask | std::ios::failbit | std::ios::badbit);
                try
                {
                    file_.open(filename, std::ios::binary | mode);
                }
                catch (std::system_error& e)
                {
                    ec = e.code();
                }
                file_.exceptions(excMask);
            }

            /**
             * @brief Set the Read Range.
             *
             * @param start
             * @param end
             */
            void setReadRanges(Ranges const& ranges, std::string_view contentType)
            {
                if (ranges.ranges.empty())
                    throw std::invalid_argument("Ranges must not be empty");

                if (ranges.ranges.size() > 1)
                {
                    std::size_t totalFile = fileSize();
                    sequences_.resize(ranges.ranges.size() + 1);

                    if (ranges.ranges.empty())
                        throw std::runtime_error("No ranges specified");

                    for (auto const& range : ranges.ranges)
                    {
                        if (range.end < range.start)
                            throw std::invalid_argument("end < start");
                        if (range.end > fileSize())
                            throw std::invalid_argument("range.end > file size");
                    }

                    std::transform(
                        ranges.ranges.begin(), ranges.ranges.end(), sequences_.rbegin(), [&, this](auto const& range) {
                            auto seq = Sequence{range.start, range.end, totalFile, contentType, splitter_};
                            totalSize_ += seq.headSection.size() + (range.end - range.start);
                            return seq;
                        });
                    sequences_.rbegin()->headSection =
                        sequences_.rbegin()->headSection.substr(2, sequences_.rbegin()->headSection.size() - 2);
                    *sequences_.begin() = Sequence{splitter_};
                    totalSize_ += splitter_.size();
                }
                else
                {
                    sequences_.resize(1);
                    sequences_.front() = Sequence{ranges.ranges.front().start, ranges.ranges.front().end};
                    totalSize_ = ranges.ranges.front().end - ranges.ranges.front().start;
                    file_.seekg(static_cast<std::streamoff>(ranges.ranges.front().start));
                }
            }

            /**
             * @brief Reads some of the multipart data into the buffer.
             *
             * @param buf
             * @param amount
             * @return std::size_t
             */
            std::size_t read(char* buf, std::size_t amount)
            {
                std::uint64_t origAmount = amount;
                auto& current = sequences_.back();

                if (current.headConsumed != current.headSection.size())
                {
                    auto copied = current.headSection.copy(
                        buf, std::min(static_cast<std::uint64_t>(amount), static_cast<std::uint64_t>(current.headSection.size()) - current.headConsumed), current.headConsumed);
                    buf += copied;
                    current.headConsumed += copied;
                    consumed_ += copied;
                    amount -= copied;
                    if (amount > 0)
                        file_.seekg(static_cast<std::streamoff>(current.start));
                }
                file_.read(buf, static_cast<std::streamoff>(std::min(static_cast<std::uint64_t>(amount), current.end - current.start)));
                auto gcount = static_cast<std::uint64_t>(file_.gcount());
                consumed_ += gcount;
                current.start += gcount;
                amount -= gcount;
                if (current.start == current.end)
                    sequences_.pop_back();

                if (sequences_.empty())
                    return (origAmount - amount); // amount should be 0 at this point.
                if (amount > 0)
                    return (origAmount - amount) + read(buf + gcount, amount);
                return origAmount;
            }

            /**
             * @brief Writes the given buffer to the file.
             *
             * @param buf
             * @param amount
             * @param error
             * @return std::size_t
             */
            std::streamsize write(char const* buf, std::streamsize amount, bool& error)
            {
                file_.write(buf, amount);
                // totalBytesProcessed_ += amount;
                if (file_.fail())
                    error = true;
                return amount;
            }

            /**
             * @brief Resets the file stream with a new stream.
             *
             * @param file A fresh fstream.
             */
            void reset(std::fstream&& file)
            {
                if (isOpen())
                    file_.close();
                file_ = std::move(file);
            }

            /**
             * @brief Resets the fstream state and goes back to the start.
             */
            void reset()
            {
                file_.clear();
                file_.seekg(0, std::ios::beg);
            }

            std::string boundary() const
            {
                return splitter_;
            }

          private:
            std::fstream file_{};
            // are inserted in reverse order, so we can pop_back.
            std::vector<Sequence> sequences_;
            std::string splitter_;
            std::uint64_t totalSize_;
            std::uint64_t consumed_;
        };

        constexpr static std::uint64_t bufferSize()
        {
            using namespace MemoryLiterals;
            return 1_MemoryPage;
        }

        template <unsigned Size>
        struct Buffer
        {
            char buf_[Size];
        };
    }

    /**
     * @brief This body is a file body, but with range request support.
     */
    class RangeFileBody
    {
      public:
        using value_type = Detail::RangeFileBodyImpl;
        using const_buffers_type = boost::asio::const_buffer;

        class reader
        {
          public:
            template <bool isRequest, class Fields>
            reader(boost::beast::http::header<isRequest, Fields>&, value_type&);
            void init(boost::optional<std::uint64_t> const&, boost::beast::error_code& ec);
            template <class ConstBufferSequence>
            std::size_t put(ConstBufferSequence const& buffers, boost::beast::error_code& ec);
            void finish(boost::beast::error_code& ec);

          private:
            value_type& body_;
        };
        class writer : private Detail::Buffer<Detail::bufferSize()>
        {
          public:
            using const_buffers_type = boost::asio::const_buffer;

            template <bool isRequest, class Fields>
            writer(boost::beast::http::header<isRequest, Fields>&, value_type&);
            void init(boost::beast::error_code& ec);
            boost::optional<std::pair<const_buffers_type, bool>> get(boost::beast::error_code& ec);

          private:
            value_type& body_;
        };

        static std::uint64_t size(value_type const& vt)
        {
            return vt.size();
        }
    };

    template <bool isRequest, class Fields>
    inline RangeFileBody::reader::reader(boost::beast::http::header<isRequest, Fields>&, value_type& bodyValue)
        : body_{bodyValue}
    {}
    inline void RangeFileBody::reader::init(boost::optional<std::uint64_t> const&, boost::beast::error_code& ec)
    {
        BOOST_ASSERT(body_.isOpen());
        ec = {};
    }
    template <class ConstBufferSequence>
    inline std::size_t RangeFileBody::reader::put(ConstBufferSequence const& buffers, boost::beast::error_code& ec)
    {
        std::size_t writtenBytes = 0;
        for (auto it = boost::asio::buffer_sequence_begin(buffers); it != boost::asio::buffer_sequence_end(buffers);
             ++it)
        {
            // Write this buffer to the file
            boost::asio::const_buffer buffer = *it;
            bool fail{false};
            writtenBytes += static_cast<std::size_t>(body_.write(
                reinterpret_cast<char const*>(buffer.data()), static_cast<std::streamsize>(buffer.size()), fail));
            if (fail)
            {
                ec = boost::beast::http::error::body_limit;
                return writtenBytes;
            }
        }

        ec = {};
        return writtenBytes;
    }
    inline void RangeFileBody::reader::finish(boost::beast::error_code& ec)
    {
        ec = {};
    }

    template <bool isRequest, class Fields>
    inline RangeFileBody::writer::writer(boost::beast::http::header<isRequest, Fields>&, value_type& bodyValue)
        : body_{bodyValue}
    {
        if (!body_.isOpen())
            throw std::invalid_argument{"File must be open"};
    }
    inline void RangeFileBody::writer::init(boost::beast::error_code& ec)
    {
        ec = {};
    }
    inline boost::optional<std::pair<RangeFileBody::const_buffers_type, bool>>
    RangeFileBody::writer::get(boost::beast::error_code& ec)
    {
        const auto remain = body_.remaining();
        const auto amount = remain > sizeof(buf_) ? sizeof(buf_) : static_cast<std::size_t>(remain);

        if (amount == 0)
        {
            ec = {};
            return boost::none;
        }

        const auto readCount = body_.read(buf_, amount);
        if (readCount == 0)
        {
            ec = boost::beast::http::error::short_read;
            return boost::none;
        }

        BOOST_ASSERT(readCount != 0);
        BOOST_ASSERT(readCount <= remain);

        ec = {};
        return {{
            const_buffers_type{buf_, readCount},
            remain > 0,
        }};
    }
}