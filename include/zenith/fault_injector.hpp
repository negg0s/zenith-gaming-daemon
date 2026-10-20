#pragma once

#include <stdexcept>
#include <string>

namespace zenith {

class FaultInjectionException : public std::runtime_error {
public:
    explicit FaultInjectionException(int point)
        : std::runtime_error("Simulated crash at fault injection point " + std::to_string(point)),
          point_(point) {}

    int get_point() const { return point_; }

private:
    int point_;
};

class FaultInjector {
public:
    static void set_target_crash_point(int point) {
        target_point() = point;
    }

    static int get_target_crash_point() {
        return target_point();
    }

    static void reset() {
        target_point() = 0;
    }

    // Called before or after each mutation
    static void trigger_if_hit(int current_point) {
        if (target_point() > 0 && current_point == target_point()) {
            throw FaultInjectionException(current_point);
        }
    }

private:
    static int& target_point() {
        static int pt = 0;
        return pt;
    }
};

} // namespace zenith
