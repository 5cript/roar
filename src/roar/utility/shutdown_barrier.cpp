#include <roar/utility/shutdown_barrier.hpp>

#include <condition_variable>
#include <csignal>
#include <cstring>
#include <exception>
#include <mutex>

namespace Roar
{
    ShutdownBarrier shutdownBarrier;

    void onSignal(int sig)
    {
        shutdownBarrier.release(sig);
    }

    struct ShutdownBarrier::Implementation
    {
        std::mutex m_conditionMutex{};
        std::condition_variable m_conditionWatcher{};
        bool m_performShutdown{false};
        int m_lastSignal{-1};
    };

    ShutdownBarrier::ShutdownBarrier() noexcept
        : m_impl{std::make_unique<Implementation>()}
    {}

    ShutdownBarrier::~ShutdownBarrier() = default;
    ShutdownBarrier::ShutdownBarrier(ShutdownBarrier&&) = default;
    ShutdownBarrier& ShutdownBarrier::operator=(ShutdownBarrier&&) = default;

    void ShutdownBarrier::release(int releasedWithWhatSignal)
    {
        {
            std::scoped_lock lock{m_impl->m_conditionMutex};
            m_impl->m_lastSignal = releasedWithWhatSignal;
            m_impl->m_performShutdown = true;
        }
        m_impl->m_conditionWatcher.notify_one();
    }

    int ShutdownBarrier::wait()
    {
        installSignalHandlers();
        std::unique_lock lock{m_impl->m_conditionMutex};
        m_impl->m_conditionWatcher.wait(lock, [this]() {
            return m_impl->m_performShutdown;
        });
        return m_impl->m_lastSignal;
    }

    void ShutdownBarrier::installSignalHandlers()
    {
        signal(SIGINT, &onSignal); // Ctrl + C
#if !defined(_WIN32)
        signal(SIGTSTP, &onSignal); // Ctrl + Z, common way to stop programs from the terminal gracefully
#endif
        signal(SIGTERM, &onSignal); // Sent by OS on shutdown
    }
}