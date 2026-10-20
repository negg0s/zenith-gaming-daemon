#include "zenith/i18n.hpp"
#include <cstdlib>
#include <cstring>

namespace zenith::i18n {

namespace {

Language current_lang = Language::English;

const std::unordered_map<std::string_view, std::string_view> ru_translations = {
    {"doctor_title", "Диагностика системы Zenith Doctor"},
    {"cgroup_v2_ok", "Контроллеры cgroups v2 активны и доступны"},
    {"cgroup_v2_fail", "cgroups v2 недоступны или отсутствуют контроллеры cpu/cpuset"},
    {"kernel_ok", "Версия ядра Linux поддерживается"},
    {"kernel_warn", "Ядро старше 5.15; некоторые функции могут быть ограничены"},
    {"privileges_ok", "Права доступа и capabilities проверены"},
    {"privileges_warn", "Недостаточно прав; требуются root или группа wheel/gamemode"},
    {"conflicts_none", "Конфликтующие сервисы не обнаружены"},
    {"conflicts_found", "Обнаружены конфликтующие сервисы оптимизации"},
    {"x3d_ok", "Драйвер AMD 3D V-Cache (amd_x3d_mode) активен"},
    {"x3d_fallback", "Драйвер AMD 3D V-Cache не найден (используется собственный L3 affinity)"},
    {"psi_ok", "Подсистема Pressure Stall Information (PSI) доступна"},
    {"psi_fail", "PSI недоступен; адаптивный контроллер не может оценивать давление"},
    {"gpu_ok", "Драйвер GPU обнаружен и поддерживается"},
    {"pipewire_ok", "Сервер PipeWire активен"},
    {"pipewire_warn", "PipeWire не запущен или сокет недоступен"},
    {"status_active_game", "Активная игра"},
    {"status_profile", "Текущий профиль"},
    {"status_tweaks_applied", "Применено твиков"},
    {"status_rollback_synced", "Журнал отката (WAL) синхронизирован"},
    {"game_none", "Нет активных игр (простой)"},
};

const std::unordered_map<std::string_view, std::string_view> en_translations = {
    {"doctor_title", "Zenith Doctor System Diagnostics"},
    {"cgroup_v2_ok", "cgroups v2 controllers active and accessible"},
    {"cgroup_v2_fail", "cgroups v2 unavailable or cpu/cpuset controllers missing"},
    {"kernel_ok", "Linux kernel version supported"},
    {"kernel_warn", "Kernel is older than 5.15; some features may be constrained"},
    {"privileges_ok", "Privileges and capabilities verified"},
    {"privileges_warn", "Insufficient privileges; root or wheel/gamemode group required"},
    {"conflicts_none", "No conflicting optimization services detected"},
    {"conflicts_found", "Conflicting optimization services detected"},
    {"x3d_ok", "AMD 3D V-Cache driver (amd_x3d_mode) active"},
    {"x3d_fallback", "AMD 3D V-Cache driver not found (using custom L3 affinity pinning fallback)"},
    {"psi_ok", "Pressure Stall Information (PSI) available"},
    {"psi_fail", "PSI unavailable; adaptive controller cannot assess pressure"},
    {"gpu_ok", "GPU driver detected and supported"},
    {"pipewire_ok", "PipeWire audio server active"},
    {"pipewire_warn", "PipeWire is not running or socket inaccessible"},
    {"status_active_game", "Active Game"},
    {"status_profile", "Current Profile"},
    {"status_tweaks_applied", "Tweaks Applied"},
    {"status_rollback_synced", "Rollback Journal (WAL) synced"},
    {"game_none", "None (Idle)"},
};

} // namespace

void init() {
    const char* lang = std::getenv("LC_ALL");
    if (!lang || !*lang) lang = std::getenv("LC_MESSAGES");
    if (!lang || !*lang) lang = std::getenv("LANG");

    if (lang && (std::strncmp(lang, "ru", 2) == 0 || std::strstr(lang, "RU") != nullptr)) {
        current_lang = Language::Russian;
    } else {
        current_lang = Language::English;
    }
}

void set_language(Language lang) {
    current_lang = lang;
}

Language get_language() {
    return current_lang;
}

std::string tr(std::string_view key) {
    if (current_lang == Language::Russian) {
        auto it = ru_translations.find(key);
        if (it != ru_translations.end()) {
            return std::string(it->second);
        }
    }
    auto it = en_translations.find(key);
    if (it != en_translations.end()) {
        return std::string(it->second);
    }
    return std::string(key);
}

std::string format_error(std::string_view code, std::string_view message) {
    std::string out = "[ZENITH-";
    out.append(code);
    out.append("] ");
    out.append(message);
    out.append(" (docs/ERRORS.md#");
    out.append(code);
    out.append(")");
    return out;
}

} // namespace zenith::i18n
