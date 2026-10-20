#pragma once

#include "zenith/common.hpp"

namespace zenith::test {

inline void create_file(const fs::path& p, std::string_view content) {
    fs::create_directories(p.parent_path());
    write_file_string(p, content);
}

inline void generate_all_fixtures(const fs::path& base_dir) {
    // 1. Ryzen 7950X3D
    {
        fs::path f = base_dir / "ryzen_7950x3d";
        create_file(f / "sys/bus/platform/drivers/amd_x3d_vcache/uevent", "DRIVER=amd_x3d_vcache\n");
        create_file(f / "sys/devices/platform/amd_x3d_vcache/amd_x3d_mode", "cache\n");
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");
        create_file(f / "sys/class/dmi/id/chassis_type", "3\n"); // Desktop

        for (int i = 0; i < 32; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            bool is_ccd0 = (i < 8) || (i >= 16 && i < 24);
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 16) + "\n");
            create_file(c / "online", "1\n");

            if (is_ccd0) {
                create_file(c / "cache/index3/size", "96M\n");
                create_file(c / "cache/index3/shared_cpu_list", "0-7,16-23\n");
                create_file(c / "cpufreq/cpuinfo_max_freq", "5250000\n");
            } else {
                create_file(c / "cache/index3/size", "32M\n");
                create_file(c / "cache/index3/shared_cpu_list", "8-15,24-31\n");
                create_file(c / "cpufreq/cpuinfo_max_freq", "5700000\n");
            }
        }
    }

    // 2. Ryzen 9950X3D (Zen 5 V-Cache asymmetry)
    {
        fs::path f = base_dir / "ryzen_9950x3d";
        create_file(f / "sys/devices/platform/amd_x3d_vcache/amd_x3d_mode", "cache\n");
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");

        for (int i = 0; i < 32; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            bool is_ccd0 = (i < 8) || (i >= 16 && i < 24);
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 16) + "\n");
            create_file(c / "online", "1\n");

            if (is_ccd0) {
                create_file(c / "cache/index3/size", "96M\n");
                create_file(c / "cache/index3/shared_cpu_list", "0-7,16-23\n");
            } else {
                create_file(c / "cache/index3/size", "32M\n");
                create_file(c / "cache/index3/shared_cpu_list", "8-15,24-31\n");
            }
        }
    }

    // 3. Ryzen 7800X3D (Single CCD with 96M V-Cache)
    {
        fs::path f = base_dir / "ryzen_7800x3d";
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");
        for (int i = 0; i < 16; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 8) + "\n");
            create_file(c / "cache/index3/size", "96M\n");
            create_file(c / "cache/index3/shared_cpu_list", "0-15\n");
            create_file(c / "online", "1\n");
        }
    }

    // 4. Ryzen 5800X3D (Zen 3 single CCD 96M)
    {
        fs::path f = base_dir / "ryzen_5800x3d";
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");
        for (int i = 0; i < 16; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 8) + "\n");
            create_file(c / "cache/index3/size", "96M\n");
            create_file(c / "cache/index3/shared_cpu_list", "0-15\n");
            create_file(c / "online", "1\n");
        }
    }

    // 5. Ryzen 5600 (Uniform 6 cores / 12 threads with 32M)
    {
        fs::path f = base_dir / "ryzen_5600";
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");
        for (int i = 0; i < 12; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 6) + "\n");
            create_file(c / "cache/index3/size", "32M\n");
            create_file(c / "cache/index3/shared_cpu_list", "0-11\n");
            create_file(c / "online", "1\n");
        }
    }

    // 6. Intel Core i9-13900K (8 P-cores + 16 E-cores)
    {
        fs::path f = base_dir / "intel_13900k";
        create_file(f / "sys/devices/system/cpu/smt/control", "on\n");
        for (int i = 0; i < 32; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "online", "1\n");

            if (i < 16) {
                create_file(c / "topology/core_type", "0x20\n"); // P-Core
                create_file(c / "topology/core_id", std::to_string(i / 2) + "\n");
                create_file(c / "cpufreq/cpuinfo_max_freq", "5800000\n");
            } else {
                create_file(c / "topology/core_type", "0x40\n"); // E-Core
                create_file(c / "topology/core_id", std::to_string(8 + (i - 16)) + "\n");
                create_file(c / "cpufreq/cpuinfo_max_freq", "4300000\n");
            }
        }
    }

    // 7. Intel Core i7-12700K (8 P-cores + 4 E-cores)
    {
        fs::path f = base_dir / "intel_12700k";
        for (int i = 0; i < 20; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "online", "1\n");

            if (i < 16) {
                create_file(c / "topology/core_type", "0x20\n"); // P-Core
                create_file(c / "topology/core_id", std::to_string(i / 2) + "\n");
            } else {
                create_file(c / "topology/core_type", "0x40\n"); // E-Core
                create_file(c / "topology/core_id", std::to_string(8 + (i - 16)) + "\n");
            }
        }
    }

    // 8. Intel Core Ultra 9 (P-cores, E-cores, LP-E cores)
    {
        fs::path f = base_dir / "intel_core_ultra_9";
        for (int i = 0; i < 22; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "online", "1\n");

            if (i < 12) {
                create_file(c / "topology/core_type", "0x20\n"); // P-Core
            } else if (i < 20) {
                create_file(c / "topology/core_type", "0x40\n"); // E-Core
            } else {
                create_file(c / "topology/core_type", "0x10\n"); // LP-E Core
            }
        }
    }

    // 9. EPYC 2xNUMA (Multi-socket / Dual-NUMA)
    {
        fs::path f = base_dir / "epyc_2x_numa";
        create_file(f / "sys/devices/system/node/node0/uevent", "NODE=0\n");
        create_file(f / "sys/devices/system/node/node1/uevent", "NODE=1\n");

        for (int i = 0; i < 32; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            int pkg = (i < 16) ? 0 : 1;
            create_file(c / "topology/physical_package_id", std::to_string(pkg) + "\n");
            create_file(c / "online", "1\n");
        }
    }

    // 10. VM 4 vCPU (Hypervisor / QEMU)
    {
        fs::path f = base_dir / "vm_4vcpu";
        create_file(f / "sys/class/dmi/id/product_name", "Standard PC (Q35 + ICH9, 2009)\n");
        create_file(f / "sys/class/dmi/id/sys_vendor", "QEMU\n");

        for (int i = 0; i < 4; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "online", "1\n");
        }
    }

    // 11. Steam Deck APU (Handheld & Battery)
    {
        fs::path f = base_dir / "steam_deck";
        create_file(f / "sys/class/dmi/id/product_name", "Jupiter\n");
        create_file(f / "sys/class/dmi/id/chassis_type", "31\n"); // Handheld
        create_file(f / "sys/class/power_supply/BAT0/type", "Battery\n");
        create_file(f / "sys/class/power_supply/BAT0/status", "Discharging\n");

        for (int i = 0; i < 8; ++i) {
            fs::path c = f / ("sys/devices/system/cpu/cpu" + std::to_string(i));
            create_file(c / "topology/physical_package_id", "0\n");
            create_file(c / "topology/core_id", std::to_string(i % 4) + "\n");
            create_file(c / "cache/index3/size", "16M\n");
            create_file(c / "online", "1\n");
        }
    }
}

} // namespace zenith::test
