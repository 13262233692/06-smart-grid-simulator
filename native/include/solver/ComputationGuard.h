#pragma once
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>

namespace GridSolver {

class CancellationToken {
public:
    CancellationToken() : m_cancelled(false), m_timeoutMs(0) {}
    explicit CancellationToken(double timeoutMs) : m_cancelled(false), m_timeoutMs(timeoutMs) {
        if (m_timeoutMs > 0) {
            m_startTime = std::chrono::steady_clock::now();
        }
    }

    CancellationToken(const CancellationToken& other)
        : m_cancelled(other.m_cancelled.load(std::memory_order_acquire))
        , m_timeoutMs(other.m_timeoutMs)
        , m_startTime(other.m_startTime) {}

    CancellationToken& operator=(const CancellationToken& other) {
        if (this != &other) {
            m_cancelled.store(other.m_cancelled.load(std::memory_order_acquire), std::memory_order_release);
            m_timeoutMs = other.m_timeoutMs;
            m_startTime = other.m_startTime;
        }
        return *this;
    }

    void cancel() { m_cancelled.store(true, std::memory_order_release); }
    bool isCancelled() const { return m_cancelled.load(std::memory_order_acquire); }

    bool isTimeout() const {
        if (m_timeoutMs <= 0) return false;
        auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - m_startTime).count();
        return elapsed > m_timeoutMs;
    }

    bool shouldStop() const { return isCancelled() || isTimeout(); }

    void throwIfCancelled() const {
        if (isCancelled()) throw std::runtime_error("Computation cancelled");
        if (isTimeout()) throw std::runtime_error("Computation timeout (" +
            std::to_string(static_cast<int>(m_timeoutMs)) + "ms)");
    }

private:
    std::atomic<bool> m_cancelled;
    double m_timeoutMs;
    std::chrono::steady_clock::time_point m_startTime;
};

struct IterationProgress {
    int currentIteration;
    int maxIterations;
    double maxMismatch;
    double tolerance;
    double elapsedMs;
};

using ProgressCallback = std::function<void(const IterationProgress&)>;

class ComputationGuard {
public:
    explicit ComputationGuard(std::mutex& mtx, CancellationToken& token)
        : m_mutex(mtx), m_token(token), m_locked(false) {
        m_token.throwIfCancelled();
        m_mutex.lock();
        m_locked = true;
        m_token.throwIfCancelled();
    }

    ~ComputationGuard() {
        if (m_locked) {
            m_mutex.unlock();
        }
    }

    ComputationGuard(const ComputationGuard&) = delete;
    ComputationGuard& operator=(const ComputationGuard&) = delete;
    ComputationGuard(ComputationGuard&& other) noexcept
        : m_mutex(other.m_mutex), m_token(other.m_token), m_locked(other.m_locked) {
        other.m_locked = false;
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

    bool isLocked() const { return m_locked; }

    void checkPoint() const { m_token.throwIfCancelled(); }

private:
    std::mutex& m_mutex;
    CancellationToken& m_token;
    bool m_locked;
};

class SolverState {
public:
    SolverState() : m_computing(false), m_requestId(0) {}

    bool isComputing() const { return m_computing.load(std::memory_order_acquire); }

    uint64_t beginComputation() {
        m_computing.store(true, std::memory_order_release);
        return ++m_requestId;
    }

    void endComputation() {
        m_computing.store(false, std::memory_order_release);
    }

    uint64_t currentRequestId() const { return m_requestId.load(std::memory_order_acquire); }

    bool isStaleRequest(uint64_t requestId) const {
        return requestId < m_requestId.load(std::memory_order_acquire);
    }

    CancellationToken& cancellationToken() { return m_cancellationToken; }

    void cancelCurrent() { m_cancellationToken.cancel(); }

    void resetCancellationToken(double timeoutMs = 0) {
        m_cancellationToken = CancellationToken(timeoutMs);
    }

    std::mutex& resultMutex() { return m_resultMutex; }

private:
    std::atomic<bool> m_computing;
    std::atomic<uint64_t> m_requestId;
    CancellationToken m_cancellationToken;
    std::mutex m_resultMutex;
};

} // namespace GridSolver
