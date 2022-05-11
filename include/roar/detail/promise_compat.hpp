#pragma once

#include <functional>
#include <promise-cpp/promise.hpp>

namespace Roar::Detail
{
    template <typename T>
    struct PromiseReferenceWrap : std::reference_wrapper<T>
    {
        using std::reference_wrapper<T>::reference_wrapper;
        using std::reference_wrapper<T>::get;
        using std::reference_wrapper<T>::operator T&;
        // dummy function to prevent the compiler from complaining about func traits in promise lib.
        void operator()() const
        {}
    };

    template <typename T>
    auto ref(T& thing)
    {
        return PromiseReferenceWrap<T>{thing};
    }

    template <typename T>
    auto cref(T const& thing)
    {
        return PromiseReferenceWrap<T const>{thing};
    }

    template <typename T>
    struct UnpackReferenceWrapper
    {
        using type = T;
        static T& unpack(T& thing)
        {
            return thing;
        }
    };

    template <typename T>
    struct UnpackReferenceWrapper<PromiseReferenceWrap<T>>
    {
        using type = T&;
        static T& unpack(PromiseReferenceWrap<T> thing)
        {
            return thing.get();
        }
    };

    template <typename T>
    struct UnpackReferenceWrapper<PromiseReferenceWrap<T const>>
    {
        using type = T const&;
        static T const& unpack(PromiseReferenceWrap<T const> thing)
        {
            return thing.get();
        }
    };

    template <typename... List>
    struct PromiseTypeBindThen
    {};

    template <typename... List>
    struct PromiseTypeBindFail
    {};

    template <typename...>
    class PromiseTypeBind;

    class PromiseTypeBindBase
    {
      public:
        PromiseTypeBindBase(promise::Promise promise)
            : promise_(std::move(promise))
        {}
        virtual ~PromiseTypeBindBase() = default;

      protected:
        promise::Promise promise_;
    };

    template <typename... ThenArgs>
    struct PromiseTypeBind<PromiseTypeBindThen<ThenArgs...>> : public PromiseTypeBindBase
    {
        using PromiseTypeBindBase::PromiseTypeBindBase;

        auto then(std::function<void(typename UnpackReferenceWrapper<ThenArgs>::type...)> func)
        {
            return promise_.then([func = std::move(func)](ThenArgs... args) {
                func(UnpackReferenceWrapper<ThenArgs>::unpack(args)...);
            });
        }

        auto fail(auto&& func)
        {
            return promise_.fail(std::forward<decltype(func)>(func));
        }
    };

    template <typename... FailArgs>
    struct PromiseTypeBind<PromiseTypeBindFail<FailArgs...>> : public PromiseTypeBindBase
    {
        using PromiseTypeBindBase::PromiseTypeBindBase;

        auto then(auto&& func)
        {
            return promise_.then(std::forward<decltype(func)>(func));
        }

        auto fail(std::function<void(typename UnpackReferenceWrapper<FailArgs>::type...)> func)
        {
            return promise_.fail([func = std::move(func)](FailArgs... args) {
                func(UnpackReferenceWrapper<FailArgs>::unpack(args)...);
            });
        }
    };

    template <typename... ThenArgs, typename... FailArgs>
    struct PromiseTypeBind<PromiseTypeBindThen<ThenArgs...>, PromiseTypeBindFail<FailArgs...>>
        : public PromiseTypeBindBase
    {
        using PromiseTypeBindBase::PromiseTypeBindBase;

        PromiseTypeBind<PromiseTypeBindFail<FailArgs...>>
        then(std::function<void(typename UnpackReferenceWrapper<ThenArgs>::type...)> func)
        {
            return promise_.then([func = std::move(func)](ThenArgs... args) {
                func(UnpackReferenceWrapper<ThenArgs>::unpack(args)...);
            });
        }

        PromiseTypeBind<PromiseTypeBindThen<ThenArgs...>>
        fail(std::function<void(typename UnpackReferenceWrapper<FailArgs>::type...)> func)
        {
            return promise_.fail([func = std::move(func)](FailArgs... args) {
                func(UnpackReferenceWrapper<FailArgs>::unpack(args)...);
            });
        }
    };
}