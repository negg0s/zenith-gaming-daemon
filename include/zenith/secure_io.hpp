#pragma once

#include "zenith/common.hpp"

namespace zenith {

class SecureIo {
public:
    // Opens a file strictly beneath trusted base directory, enforcing NO_SYMLINKS / RESOLVE_BENEATH
    static int open_beneath(const fs::path& base_dir,
                            const std::string& relative_path,
                            int flags,
                            mode_t mode = 0644);

    static bool write_string(const fs::path& base_dir,
                             const std::string& relative_path,
                             std::string_view content);

    static std::optional<std::string> read_string(const fs::path& base_dir,
                                                  const std::string& relative_path);

    // Resolves an absolute path into trusted base and validated relative subpath
    static std::pair<fs::path, std::string> split_trusted_path(const fs::path& absolute_path);

    static bool safe_write_file(const fs::path& path, std::string_view content);
    static std::optional<std::string> safe_read_file(const fs::path& path);
};

} // namespace zenith
