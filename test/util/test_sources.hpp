#pragma once

#include <roar/curl/source.hpp>

#include <functional>
#include <numeric>
#include <string>

namespace Roar::Tests
{
    class LetterGenerator
    {
      public:
        LetterGenerator()
            : m_offset{-1}
            , m_pregeneratedBuffer(26, 'x')
        {
            std::iota(begin(m_pregeneratedBuffer), end(m_pregeneratedBuffer), 'A');
        }

        char operator()()
        {
            return m_pregeneratedBuffer[static_cast<std::size_t>(++m_offset % 26)];
        }

      private:
        long long m_offset;
        std::string m_pregeneratedBuffer;
    };

    class BigSource : public Curl::Source
    {
      public:
        BigSource(std::size_t size, std::function<char()> generator = LetterGenerator{})
            : m_size{size}
            , m_generator{std::move(generator)}
        {}

        std::size_t fetch(char* buffer, std::size_t amount) override
        {
            for (std::size_t i = 0; i != amount; ++i)
                buffer[i] = m_generator();
            return amount;
        }
        std::size_t size() override
        {
            return m_size;
        }

      private:
        std::size_t m_size;
        std::function<char()> m_generator;
    };

    class ChunkedSource : public Curl::Source
    {
      public:
        ChunkedSource(std::size_t size, std::function<char()> generator = LetterGenerator{})
            : m_size{size}
            , m_generator{std::move(generator)}
        {}

        std::size_t fetch(char* buffer, std::size_t amount) override
        {
            const auto remainingBytes = std::min(amount, m_size);
            for (std::size_t i = 0; i != remainingBytes; ++i)
                buffer[i] = m_generator();
            m_size -= remainingBytes;
            return remainingBytes;
        }
        std::size_t size() override
        {
            return 0;
        }

        bool isChunked() const override
        {
            return true;
        }

      private:
        std::size_t m_size;
        std::function<char()> m_generator;
    };

}