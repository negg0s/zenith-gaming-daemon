// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

namespace zenith {

enum class DoctorStatus {
    Pass,
    Warning,
    Fail
};

struct DoctorItem {
    std::string name;
    DoctorStatus status;
    std::string message;
    std::string advice;
    std::string fix_command;
};

class Doctor {
public:
    static std::vector<DoctorItem> run_all_checks();
    static void print_report(const std::vector<DoctorItem>& items);
    static std::string to_json(const std::vector<DoctorItem>& items);

    // Individual checks
    static DoctorItem check_cgroups_v2();
    static DoctorItem check_kernel_version();
    static DoctorItem check_privileges();
    static DoctorItem check_conflicting_services();
    static DoctorItem check_x3d_driver();
    static DoctorItem check_psi();
    static DoctorItem check_gpu();
    static DoctorItem check_pipewire();
    static DoctorItem check_max_map_count();
};

} // namespace zenith
