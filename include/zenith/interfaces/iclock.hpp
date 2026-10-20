#pragma once

#include <chrono>
#include <thread>
#include <mutex>

namespace zenith {

class IClock {
public:
    virtual ~IClock() = default;

    [[nodiscard]] virtual std::chrono::steady_clock::time_point now_steady() const = 0;
    [[nodiscard]] virtual std::chrono::system_clock::time_point now_system() const = 0;
    virtual void sleep_for(std::chrono::milliseconds ms) const = 0;
};

class RealClock : public IClock {
public:
    std::chrono::steady_clock::time_point now_steady() const override {
        return std::chrono::steady_clock::now();
    }

    std::chrono::system_clock::time_point now_system() const override {
        return std::chrono::system_clock::now();
    }

    void sleep_for(std::chrono::milliseconds ms) const override {
        std::this_thread::sleep_for(ms);
    }
};

class MockClock : public IClock {
public:
    explicit MockClock(std::chrono::steady_clock::time_point initial_steady = std::chrono::steady_clock::time_point{},
                       std::chrono::system_clock::time_point initial_system = std::chrono::system_clock::time_point{})
        : steady_now_(initial_steady), system_now_(initial_system) {}

    std::chrono::steady_clock::time_point now_steady() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return steady_now_;
    }

    std::chrono::system_clock::time_point now_system() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return system_now_;
    }

    void sleep_for(std::chrono::milliseconds ms) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        steady_now_ += ms;
        system_now_ += ms;
    }

    void advance(std::chrono::milliseconds ms) {
        sleep_for(ms);
    }

private:
    mutable std::mutex mutex_;
    mutable std::chrono::steady_clock::time_point steady_now_;
    mutable std::chrono::system_clock::time_point system_now_;
};

} // namespace zenith
