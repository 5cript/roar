#pragma once

namespace Roar::Detail
{
    /**
     * @brief Utilitarian class to store and modify strings in compile time.
     *
     * @tparam Size
     */
    template <unsigned Size>
    class FixedString
    {
      public:
        constexpr FixedString()
        {}
        constexpr FixedString(char const* s)
        {
            for (unsigned i = 0; i != Size; ++i)
                m_buffer[i] = s[i];
        }
        constexpr operator char const*() const
        {
            return m_buffer;
        }

        /**
         * @brief glibc strcmp implementation.
         */
        template <unsigned OtherSize>
        constexpr int compare(FixedString<OtherSize> const& other) const
        {
            auto const* s1 = &m_buffer[0];
            auto const* s2 = &other.m_buffer[0];
            unsigned char c1, c2;
            do
            {
                c1 = static_cast<unsigned char>(*s1++);
                c2 = static_cast<unsigned char>(*s2++);
                if (c1 == '\0')
                {
                    return c1 - c2;
                }
            } while (c1 == c2);
            return c1 - c2;
        }

        constexpr static auto m_size = Size;
        char m_buffer[Size + 1]{};
    };
    template <unsigned N>
    FixedString(char const (&)[N]) -> FixedString<N - 1>;

    /**
     * @brief Allows for compile-time string concatenation. Use very sparingly.
     */
    template <unsigned... Length>
    constexpr auto FixConcat(const char (&... strings)[Length])
    {
        constexpr unsigned Count = (... + Length) - sizeof...(Length);
        FixedString<Count + 1> result = {};
        result.m_buffer[Count] = '\0';

        char* dst = result.m_buffer;
        for (const char* src : {strings...})
        {
            for (; *src != '\0'; ++src, ++dst)
            {
                *dst = *src;
            }
        }
        return result;
    }
}