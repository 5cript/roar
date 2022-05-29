#pragma once

#include <memory>

namespace Roar
{
    /**
     * A ShutdownBarrier will block execution until SIGINT (Ctrl + C), SIGTSTP (Ctrl + Z) or SIGTERM (OS
     * shutdown and graceful termination signal from task managers) is received.
     */
    class ShutdownBarrier
    {
      public:
        ShutdownBarrier() noexcept;
        ~ShutdownBarrier();
        ShutdownBarrier(ShutdownBarrier&&);
        ShutdownBarrier(ShutdownBarrier const&) = delete;
        ShutdownBarrier& operator=(ShutdownBarrier&&);
        ShutdownBarrier& operator=(ShutdownBarrier const&) = delete;

        /// Releases the barrier and stops blocking
        void release(int releasedWithWhatSignal);

        /// Waits until release is called and returns the signal.
        int wait();

      private:
        static void installSignalHandlers();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> m_impl;
    };

    extern ShutdownBarrier shutdownBarrier;
}