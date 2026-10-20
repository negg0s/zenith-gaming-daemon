// SPDX-License-Identifier: MIT
#include "zenith/local_learner.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace zenith {

namespace {

std::vector<std::string> split_csv_line(std::string_view line) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : line) {
        if (c == ',' || c == ';') {
            tokens.push_back(trim(token));
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(trim(token));
    return tokens;
}

} // namespace

MangoHudBenchmark LocalLearner::parse_mangohud_csv(std::string_view csv_content) {
    MangoHudBenchmark bm;
    std::string content_str(csv_content);
    std::istringstream stream(content_str);
    std::string line;

    std::vector<double> frametimes;
    int fps_col = -1;
    int ft_col = -1;
    bool header_parsed = false;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tokens = split_csv_line(line);
        if (tokens.empty()) continue;

        if (!header_parsed) {
            for (size_t i = 0; i < tokens.size(); ++i) {
                std::string ltok = tokens[i];
                std::transform(ltok.begin(), ltok.end(), ltok.begin(), ::tolower);
                if (ltok == "fps") fps_col = static_cast<int>(i);
                else if (ltok == "frametime" || ltok == "frame_time") ft_col = static_cast<int>(i);
            }
            if (fps_col >= 0 || ft_col >= 0) {
                header_parsed = true;
                continue;
            }
            // If no recognized header, assume column 0 is fps or frametime
            header_parsed = true;
        }

        double ft_val = 0.0;
        try {
            if (ft_col >= 0 && static_cast<size_t>(ft_col) < tokens.size()) {
                ft_val = std::stod(tokens[ft_col]);
                // If frametime is in microseconds (> 1000), convert to ms
                if (ft_val > 500.0) ft_val /= 1000.0;
            } else if (fps_col >= 0 && static_cast<size_t>(fps_col) < tokens.size()) {
                double fps = std::stod(tokens[fps_col]);
                if (fps > 0.0) ft_val = 1000.0 / fps;
            } else if (!tokens.empty()) {
                double val = std::stod(tokens[0]);
                if (val > 0.0) {
                    ft_val = (val > 50.0) ? (1000.0 / val) : val;
                }
            }
        } catch (...) {
            continue;
        }

        if (ft_val > 0.1 && ft_val < 500.0) {
            frametimes.push_back(ft_val);
        }
    }

    if (frametimes.empty()) return bm;

    bm.sample_count = static_cast<uint32_t>(frametimes.size());

    // Compute average frametime and variance
    double sum_ft = 0.0;
    for (double ft : frametimes) sum_ft += ft;
    double avg_ft = sum_ft / frametimes.size();
    bm.avg_fps = (avg_ft > 0.0) ? (1000.0 / avg_ft) : 0.0;

    double variance_sum = 0.0;
    for (double ft : frametimes) {
        double diff = ft - avg_ft;
        variance_sum += (diff * diff);
        if (ft > (avg_ft * 2.0) || ft > 33.3) {
            bm.stutter_count++;
        }
    }
    bm.frametime_variance_ms = std::sqrt(variance_sum / frametimes.size());

    // Sort to compute percentiles (1% low, 0.1% low)
    std::vector<double> sorted_ft = frametimes;
    std::sort(sorted_ft.begin(), sorted_ft.end());

    size_t idx_p1 = static_cast<size_t>(std::floor(sorted_ft.size() * 0.99));
    if (idx_p1 >= sorted_ft.size()) idx_p1 = sorted_ft.size() - 1;
    bm.p1_low_fps = (sorted_ft[idx_p1] > 0.0) ? (1000.0 / sorted_ft[idx_p1]) : 0.0;

    size_t idx_p01 = static_cast<size_t>(std::floor(sorted_ft.size() * 0.999));
    if (idx_p01 >= sorted_ft.size()) idx_p01 = sorted_ft.size() - 1;
    bm.p01_low_fps = (sorted_ft[idx_p01] > 0.0) ? (1000.0 / sorted_ft[idx_p01]) : 0.0;

    return bm;
}

void LocalLearner::record_session(GameSessionRecord record) {
    records_[record.game_id].push_back(std::move(record));
}

ProfileRecommendation LocalLearner::suggest_profile(
    const std::string& game_id,
    bool on_battery,
    bool is_handheld
) const {
    ProfileRecommendation rec;
    rec.game_id = game_id;

    // Platform power overrides
    if (on_battery) {
        rec.suggested_profile = Profile::Balanced;
        rec.explanation = "System is operating on battery power. Balanced profile recommended to maximize playtime.";
        rec.confidence_score = 0.95;
        return rec;
    }
    if (is_handheld) {
        rec.suggested_profile = Profile::Balanced;
        rec.explanation = "Handheld device form factor detected. Balanced profile prevents thermal saturation and fan noise.";
        rec.confidence_score = 0.90;
        return rec;
    }

    auto it = records_.find(game_id);
    if (it == records_.end() || it->second.empty()) {
        rec.suggested_profile = Profile::Balanced;
        rec.explanation = "No local benchmark sessions recorded for this title. Balanced is the safe recommended baseline.";
        rec.confidence_score = 0.50;
        return rec;
    }

    const auto& sessions = it->second;

    // Check if aggressive caused thermal throttling or excessive jitter
    const GameSessionRecord* aggressive_sess = nullptr;
    const GameSessionRecord* balanced_sess = nullptr;

    for (const auto& s : sessions) {
        if (s.profile_used == Profile::Aggressive) aggressive_sess = &s;
        if (s.profile_used == Profile::Balanced) balanced_sess = &s;
    }

    if (aggressive_sess && aggressive_sess->thermal_throttled) {
        rec.suggested_profile = Profile::Balanced;
        rec.explanation = "Aggressive profile caused thermal throttling on this game (Max Temp: " +
                          std::to_string(aggressive_sess->max_cpu_temp) +
                          "C). Balanced profile provides smoother frame pacing without throttling.";
        rec.confidence_score = 0.90;
        return rec;
    }

    if (aggressive_sess && balanced_sess) {
        // Compare frametime variance and 1% lows
        if (aggressive_sess->benchmark.frametime_variance_ms > (balanced_sess->benchmark.frametime_variance_ms * 1.25)) {
            rec.suggested_profile = Profile::Balanced;
            rec.explanation = "Aggressive profile resulted in 25%+ higher frametime variance (micro-stutter). Balanced profile recommended.";
            rec.confidence_score = 0.85;
            return rec;
        }

        if (aggressive_sess->benchmark.p1_low_fps > (balanced_sess->benchmark.p1_low_fps * 1.05)) {
            rec.suggested_profile = Profile::Aggressive;
            rec.explanation = "Aggressive profile reliably elevates 1% low FPS by " +
                              std::to_string(static_cast<int>((aggressive_sess->benchmark.p1_low_fps / balanced_sess->benchmark.p1_low_fps - 1.0) * 100)) +
                              "% with acceptable thermal headroom.";
            rec.confidence_score = 0.88;
            return rec;
        }
    }

    rec.suggested_profile = Profile::Balanced;
    rec.explanation = "Balanced profile delivers optimal efficiency and stable frame pacing.";
    rec.confidence_score = 0.75;
    return rec;
}

std::string LocalLearner::format_suggestion_text(const ProfileRecommendation& rec) const {
    std::string text = "Zenith Profile Suggestion for [" + rec.game_id + "]:\n";
    text += "  Recommended Profile: " + profile_to_string(rec.suggested_profile) + "\n";
    text += "  Confidence Score:    " + std::to_string(static_cast<int>(rec.confidence_score * 100)) + "%\n";
    text += "  Reasoning:           " + rec.explanation + "\n";
    text += "  Telemetry:           100% Offline (Local database only, zero external data collection)\n";
    return text;
}

const std::vector<GameSessionRecord>& LocalLearner::get_sessions(const std::string& game_id) const {
    static const std::vector<GameSessionRecord> empty;
    auto it = records_.find(game_id);
    return (it != records_.end()) ? it->second : empty;
}

size_t LocalLearner::total_sessions_recorded() const {
    size_t count = 0;
    for (const auto& [_, list] : records_) count += list.size();
    return count;
}

bool LocalLearner::save_to_file(const fs::path& path) const {
    std::string json = "{\n  \"games\": {\n";
    size_t g_idx = 0;
    for (const auto& [game_id, list] : records_) {
        json += "    \"" + game_id + "\": [\n";
        for (size_t s_idx = 0; s_idx < list.size(); ++s_idx) {
            const auto& s = list[s_idx];
            json += "      {\n";
            json += "        \"profile\": \"" + profile_to_string(s.profile_used) + "\",\n";
            json += "        \"avg_fps\": " + std::to_string(s.benchmark.avg_fps) + ",\n";
            json += "        \"p1_low_fps\": " + std::to_string(s.benchmark.p1_low_fps) + ",\n";
            json += "        \"variance_ms\": " + std::to_string(s.benchmark.frametime_variance_ms) + ",\n";
            json += "        \"stutters\": " + std::to_string(s.benchmark.stutter_count) + ",\n";
            json += "        \"throttled\": " + std::string(s.thermal_throttled ? "true" : "false") + "\n";
            json += "      }" + std::string((s_idx + 1 < list.size()) ? "," : "") + "\n";
        }
        json += "    ]" + std::string((g_idx + 1 < records_.size()) ? "," : "") + "\n";
        g_idx++;
    }
    json += "  }\n}\n";

    return write_file_string(path, json);
}

bool LocalLearner::load_from_file(const fs::path& path) {
    auto content = read_file_string(path);
    if (!content) return false;
    // Simple verification that file exists and contains valid JSON markers
    return content->find("\"games\"") != std::string::npos;
}

} // namespace zenith
