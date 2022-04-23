#pragma once

namespace Roar
{
    inline namespace Literals
    {
        inline namespace MemoryLiterals
        {
            constexpr unsigned long long operator"" _Bytes(unsigned long long n)
            {
                return n;
            }

            constexpr unsigned long long operator"" _KiB(unsigned long long n)
            {
                return n * 1024ull;
            }

            constexpr unsigned long long operator"" _MiB(unsigned long long n)
            {
                return n * 1024ull * 1024ull;
            }

            constexpr unsigned long long operator"" _GiB(unsigned long long n)
            {
                return n * 1024ull * 1024ull * 1024ull;
            }

            constexpr unsigned long long operator"" _MemoryPage(unsigned long long n)
            {
                return n * 4_KiB;
            }
        }
    }
}