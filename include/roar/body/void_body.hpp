#pragma once

#include <boost/beast/http/message.hpp>

#include <cstdint>

namespace Roar
{
    /**
     * @brief This body is for reading into nothing.
     */
    class VoidBody
    {
      public:
        struct Void
        {};
        using value_type = Void;
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
        };
        class writer
        {
          public:
            template <bool isRequest, class Fields>
            writer(boost::beast::http::header<isRequest, Fields>&, value_type&);
            void init(boost::beast::error_code& ec);
            boost::optional<std::pair<const_buffers_type, bool>> get(boost::beast::error_code& ec);
        };

        static std::uint64_t size(value_type const&)
        {
            return 0;
        }
    };

    template <bool isRequest, class Fields>
    inline VoidBody::reader::reader(boost::beast::http::header<isRequest, Fields>&, value_type&)
    {}
    inline void VoidBody::reader::init(boost::optional<std::uint64_t> const&, boost::beast::error_code& ec)
    {
        ec = {};
    }
    template <class ConstBufferSequence>
    inline std::size_t VoidBody::reader::put(ConstBufferSequence const& buffers, boost::beast::error_code& ec)
    {
        ec = {};
        std::size_t writtenAmount = 0;
        for (auto it = boost::asio::buffer_sequence_begin(buffers); it != boost::asio::buffer_sequence_end(buffers);
             ++it)
        {
            writtenAmount += it->size();
        }
        return writtenAmount;
    }
    inline void VoidBody::reader::finish(boost::beast::error_code& ec)
    {
        ec = {};
    }

    template <bool isRequest, class Fields>
    inline VoidBody::writer::writer(boost::beast::http::header<isRequest, Fields>&, value_type&)
    {}
    inline void init(boost::beast::error_code& ec)
    {
        ec = {};
    }
    inline boost::optional<std::pair<VoidBody::const_buffers_type, bool>>
    VoidBody::writer::get(boost::beast::error_code& ec)
    {
        ec = {};
        return boost::none;
    }
}