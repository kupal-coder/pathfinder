#pragma once

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class GJGameLevel;

struct VerificationResult {
    bool completed = false;
    bool died = false;
    int frame = 0;
    int objectID = 0;
    float objectX = 0;
    float objectY = 0;
    float objectRotation = 0;
    float objectScaleX = 1;
    float objectScaleY = 1;
    bool player2 = false;
    float playerX = 0;
    float playerY = 0;
    double playerYVelocity = 0;
    uint64_t traceHash = 1469598103934665603ull;
    std::string error;
};

// Replays a candidate through a real PlayLayer. This must be called on the
// cocos thread; it intentionally uses Geometry Dash's object list, trigger
// graph, movement and collision callbacks rather than gd-sim.
VerificationResult verifyInGame(GJGameLevel* level, std::vector<uint8_t> const& macro);

class RuntimeSearchTask {
public:
    struct promise_type {
        std::vector<uint8_t> result;
        std::exception_ptr exception;

        RuntimeSearchTask get_return_object() {
            return RuntimeSearchTask(
                std::coroutine_handle<promise_type>::from_promise(*this)
            );
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(bool) noexcept { return {}; }
        void return_value(std::vector<uint8_t> value) { result = std::move(value); }
        void unhandled_exception() { exception = std::current_exception(); }
    };

    RuntimeSearchTask(RuntimeSearchTask const&) = delete;
    RuntimeSearchTask& operator=(RuntimeSearchTask const&) = delete;
    RuntimeSearchTask(RuntimeSearchTask&& other) noexcept
      : m_handle(std::exchange(other.m_handle, {})) {}
    RuntimeSearchTask& operator=(RuntimeSearchTask&& other) noexcept {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }
    ~RuntimeSearchTask() { if (m_handle) m_handle.destroy(); }

    bool resume() {
        if (m_handle && !m_handle.done()) m_handle.resume();
        if (m_handle && m_handle.done() && m_handle.promise().exception)
            std::rethrow_exception(m_handle.promise().exception);
        return !m_handle || m_handle.done();
    }
    std::vector<uint8_t> takeResult() {
        return m_handle ? std::move(m_handle.promise().result) : std::vector<uint8_t>{};
    }

private:
    explicit RuntimeSearchTask(std::coroutine_handle<promise_type> handle)
      : m_handle(handle) {}
    std::coroutine_handle<promise_type> m_handle;
};

// Searches by advancing and checkpointing a real PlayLayer. The coroutine
// yields after individual trials so the cocos thread retains a frame budget.
RuntimeSearchTask pathfindInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(double)> progress
);
