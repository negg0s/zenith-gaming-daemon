#pragma once

#include "zenith/tweaks/tweak_interface.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace zenith {

struct MangoHudBenchmark {
    double avg_fps = 0.0;
    double p1_low_fps = 0.0;
    double p01_low_fps = 0.0;
    double frametime_variance_ms = 0.0;
    uint32_t stutter_count = 0;
    uint32_t sample_count = 0;
};

struct GameSessionRecord {
    std::string game_id;
    Profile profile_used = Profile::Balanced;
    MangoHudBenchmark benchmark;
    double max_memory_pressure = 0.0;
    uint32_t max_cpu_temp = 0;
    bool thermal_throttled = false;
};

struct ProfileRecommendation {
    std::string game_id;
    Profile suggested_profile = Profile::Balanced;
    std::string explanation;
    double confidence_score = 0.0;
};

class LocalLearner {
public:
    LocalLearner() = default;

    // Parse MangoHud CSV log (headers: fps, frametime or raw numbers)
    [[nodiscard]] static MangoHudBenchmark parse_mangohud_csv(std::string_view csv_content);

    void record_session(GameSessionRecord record);

    [[nodiscard]] ProfileRecommendation suggest_profile(
        const std::string& game_id,
        bool on_battery = false,
        bool is_handheld = false
    ) const;

    [[nodiscard]] std::string format_suggestion_text(const ProfileRecommendation& rec) const;

    [[nodiscard]] const std::vector<GameSessionRecord>& get_sessions(const std::string& game_id) const;
    [[nodiscard]] size_t total_sessions_recorded() const;

    bool save_to_file(const fs::path& path) const;
    bool load_from_file(const fs::path& path);

private:
    std::unordered_map<std::string, std::vector<GameSessionRecord>> records_;
};

} // namespace zenith
