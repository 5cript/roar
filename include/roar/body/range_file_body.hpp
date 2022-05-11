#pragma once

#include <boost/beast/http/message.hpp>
#include <roar/literals/memory.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace Roar
{
    namespace Detail
    {
        class RangeFileBodyImpl
        {
          public:
            RangeFileBodyImpl() = default;
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
                return size_ + suffix_.size();
            }

            std::uint64_t realSize()
            {
                auto pos = file_.tellg();
                file_.seekg(0, std::ios::end);
                auto size = file_.tellg();
                file_.seekg(pos);
                return size;
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
                    originalPath_ = filename;
                }
                catch (std::system_error& e)
                {
                    ec = e.code();
                }
                file_.exceptions(excMask);
            }

            std::filesystem::path const& originalPath() const
            {
                return originalPath_;
            }

            /**
             * @brief Set the Read Range.
             *
             * @param start
             * @param end
             */
            void setReadRange(std::uint64_t start, std::uint64_t end)
            {
                file_.seekg(start);
                size_ = end - start;
            }

            /**
             * @brief Set the Write Range.
             *
             * @param start
             * @param end
             */
            void setWriteRange(std::uint64_t start, std::uint64_t end)
            {
                file_.seekp(start);
                size_ = end - start;
            }

            /**
             * @brief Reads some of the file into the buffer.
             *
             * @param buf
             * @param amount
             * @return std::size_t
             */
            std::size_t read(char* buf, std::size_t amount)
            {
                if (amount + totalBytesProcessed_ <= size_)
                {
                    file_.read(buf, amount);
                    totalBytesProcessed_ += file_.gcount();
                    return file_.gcount();
                }
                else
                {
                    auto amountToReadFromFile = 0;
                    auto amountToReadFromSuffix = amount;
                    if (size_ > totalBytesProcessed_)
                    {
                        amountToReadFromFile = size_ - totalBytesProcessed_;
                        file_.read(buf, amountToReadFromFile);
                        totalBytesProcessed_ += amountToReadFromFile;
                        amountToReadFromSuffix = amount - amountToReadFromFile;
                    }
                    std::memcpy(buf + amountToReadFromFile, suffix_.data(), amountToReadFromSuffix);
                    totalBytesProcessed_ += amountToReadFromSuffix;
                    return amountToReadFromFile + amountToReadFromSuffix;
                }
            }

            /**
             * @brief Writes the given buffer to the file.
             *
             * @param buf
             * @param amount
             * @param error
             * @return std::size_t
             */
            std::size_t write(char const* buf, std::size_t amount, bool& error)
            {
                file_.write(buf, amount);
                totalBytesProcessed_ += amount;
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

            std::size_t totalBytesProcessed() const
            {
                return totalBytesProcessed_;
            }

            void suffix(std::string suffix)
            {
                suffix_ = std::move(suffix);
            }

          private:
            std::fstream file_{};
            std::filesystem::path originalPath_{};
            std::uint64_t size_{0};
            std::uint64_t totalBytesProcessed_{0};
            std::string suffix_{};
        };

        constexpr static std::size_t bufferSize()
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
            writtenBytes += body_.write(reinterpret_cast<char const*>(buffer.data()), buffer.size(), fail);
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
        const auto remain = body_.size() - body_.totalBytesProcessed();
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