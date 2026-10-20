# Developer Guide: How to Add a New Tweak

Zenith is designed around a modular, plugin-like tweak architecture. Adding a new kernel, sysfs, or hardware optimization requires creating a single C++ source file implementing `ITweak`, declaring the participating profiles, and registering it with `ZENITH_REGISTER_TWEAK`.

---

## 1. Overview of `ITweak` Interface

Every tweak inherits from `zenith::ITweak` (defined in `include/zenith/tweaks/tweak_interface.hpp`):

```cpp
class ITweak {
public:
    virtual ~ITweak() = default;

    // Metadata
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual uint32_t supported_profiles() const = 0;

    // Lifecycle
    virtual bool probe() = 0;                 // Check hardware / kernel compatibility
    virtual bool apply(Profile profile) = 0;  // Apply target value
    virtual bool revert() = 0;                // Restore original value

    // Introspection
    [[nodiscard]] virtual std::string describe() const = 0;
    [[nodiscard]] virtual std::string explain() const = 0;
    [[nodiscard]] virtual std::string get_current_value() const = 0;
    [[nodiscard]] virtual std::string get_target_value(Profile profile) const = 0;
};
```

---

## 2. Step-by-Step Implementation

Let's walk through an example: adding a new tweak for `/proc/sys/kernel/sched_cfs_bandwidth_slice_us`.

### Step 1: Create `src/tweaks/cfs_slice_tweak.cpp`

```cpp
#include "zenith/tweaks/tweak_interface.hpp"
#include "zenith/tweaks/tweak_registry.hpp"
#include "zenith/secure_io.hpp"
#include "zenith/structured_logger.hpp"

namespace zenith {

class CfsSliceTweak : public ITweak {
public:
    std::string name() const override {
        return "sched_cfs_bandwidth_slice_us";
    }

    std::string description() const override {
        return "Tunes CFS bandwidth slice to 3000us for reduced preemption latency";
    }

    uint32_t supported_profiles() const override {
        // Participate in Balanced and Aggressive profiles
        return ProfileMask::Balanced | ProfileMask::Aggressive;
    }

    bool probe() override {
        // Probe returns true if the kernel node exists and is accessible
        return SecureIo::exists("/proc/sys/kernel/sched_cfs_bandwidth_slice_us");
    }

    bool apply(Profile profile) override {
        if (!probe()) return false;

        // Read and save baseline value for crash-safe rollback
        auto current = SecureIo::read_string("/proc/sys/kernel/sched_cfs_bandwidth_slice_us");
        if (current.is_ok()) {
            original_value_ = current.value();
        }

        std::string target = (profile == Profile::Aggressive) ? "1000" : "3000";
        auto res = SecureIo::write_string("/proc/sys/kernel/sched_cfs_bandwidth_slice_us", target);
        applied_ = res.is_ok();
        return applied_;
    }

    bool revert() override {
        if (!applied_ || original_value_.empty()) return true;
        auto res = SecureIo::write_string("/proc/sys/kernel/sched_cfs_bandwidth_slice_us", original_value_);
        applied_ = false;
        return res.is_ok();
    }

    std::string describe() const override {
        return name() + " (current: " + get_current_value() + ")";
    }

    std::string explain() const override {
        return "Smaller CFS bandwidth slice prevents long batch processing stalls.";
    }

    std::string get_current_value() const override {
        return SecureIo::read_string("/proc/sys/kernel/sched_cfs_bandwidth_slice_us").value_or("unknown");
    }

    std::string get_target_value(Profile profile) const override {
        return (profile == Profile::Aggressive) ? "1000" : "3000";
    }

private:
    std::string original_value_;
    bool applied_{false};
};

// Register tweak automatically into the global registry
ZENITH_REGISTER_TWEAK(CfsSliceTweak)

} // namespace zenith
```

### Step 2: Register in `CMakeLists.txt`

Add the new source file to `ZENITH_SOURCES` in `CMakeLists.txt`:

```cmake
set(ZENITH_SOURCES
    ...
    src/tweaks/cfs_slice_tweak.cpp
)
```

### Step 3: Add Unit Tests in `tests/test_tweaks.cpp`

Add verification tests exercising `probe()`, `apply()`, `revert()`, and `supported_profiles()`:

```cpp
void test_cfs_slice_tweak() {
    auto tweak = TweakRegistry::instance().get_tweak("sched_cfs_bandwidth_slice_us");
    assert(tweak != nullptr);
    assert(tweak->supported_profiles() & ProfileMask::Balanced);
    // Exercise mock or probe
}
```

### Step 4: Document in `docs/TWEAKS.md`

Every tweak MUST be accompanied by an entry in `docs/TWEAKS.md` covering:
* Kernel path & minimum kernel version.
* Scientific rationale & mechanism.
* Potential risks or failure conditions.
* Benchmarked impact on 1% and 0.1% FPS lows.
