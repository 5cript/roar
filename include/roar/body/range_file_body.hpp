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
                return size_;
            }

            bool isOpen() const
            {
                return file_.is_open();
            }

            void close()
            {
                file_.close();
            }

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

            void setReadRange(std::uint64_t start, std::uint64_t end)
            {
                file_.seekg(start);
                size_ = end - start;
            }

            void setWriteRange(std::uint64_t start, std::uint64_t end)
            {
                file_.seekp(start);
                size_ = end - start;
            }

            std::size_t read(char* buf, std::size_t amount)
            {
                file_.read(buf, amount);
                return file_.gcount();
            }

            std::size_t write(char const* buf, std::size_t amount, bool& error)
            {
                file_.write(buf, amount);
                if (file_.fail())
                    error = true;
                return amount;
            }

            void reset(std::fstream&& file)
            {
                if (isOpen())
                    file_.close();
                file_ = std::move(file);
            }

          private:
            std::fstream file_;
            std::uint64_t size_;
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
            std::uint64_t remain_;
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

        remain_ = body_.size();
    }
    inline void RangeFileBody::writer::init(boost::beast::error_code& ec)
    {
        ec = {};
    }
    inline boost::optional<std::pair<RangeFileBody::const_buffers_type, bool>>
    RangeFileBody::writer::get(boost::beast::error_code& ec)
    {
        const auto amount = remain_ > sizeof(buf_) ? sizeof(buf_) : static_cast<std::size_t>(remain_);

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
        BOOST_ASSERT(readCount <= remain_);

        remain_ -= readCount;

        ec = {};
        return {{
            const_buffers_type{buf_, readCount},
            remain_ > 0,
        }};
    }
}