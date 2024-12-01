#pragma once

#include <boost/preprocessor/cat.hpp>

#include <functional>
#include <type_traits>
#include <utility>

#define ROAR_SE_UNIQUE_IDENTIFIER BOOST_PP_CAT(roar_se_unique_identifier_, __COUNTER__)

namespace Roar
{
    class ScopeExit
    {
      public:
        template <typename FunctionT>
        requires std::is_nothrow_invocable_v<FunctionT>
        explicit ScopeExit(FunctionT&& func)
            : onExit_(std::forward<FunctionT>(func))
        {}
        ~ScopeExit()
        {
            if (onExit_)
                onExit_();
        }
        ScopeExit(ScopeExit&& other) = delete;
        ScopeExit& operator=(ScopeExit&& other) = delete;
        ScopeExit(const ScopeExit&) = delete;
        ScopeExit& operator=(const ScopeExit&) = delete;
        void disarm()
        {
            onExit_ = {};
        }

      private:
        std::function<void()> onExit_;
    };

    namespace Detail
    {
        struct MakeScopeExitImpl
        {
            template <typename FunctionT>
            auto operator->*(FunctionT&& fn) const
            {
                return ScopeExit{std::forward<FunctionT>(fn)};
            }
        };
    }

#define ROAR_ON_SCOPE_EXIT auto ROAR_SE_UNIQUE_IDENTIFIER = ::Roar::Detail::MakeScopeExitImpl{}->*[&]() noexcept
}