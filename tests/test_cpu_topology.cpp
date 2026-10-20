#include "zenith/cpu_topology.hpp"
#include "fixture_generator.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void test_cpu_list_parsing() {
    using namespace zenith;

    auto list1 = CpuTopology::parse_cpu_list("0-3,7,12-14");
    std::vector<int> expected1 = { 0, 1, 2, 3, 7, 12, 13, 14 };
    assert(list1 == expected1);

    CpuTopology topo;
    assert(topo.to_cpuset_string(list1) == "0-3,7,12-14");

    auto list2 = CpuTopology::parse_cpu_list("  5, 2 , 1-3 , 2  ");
    std::vector<int> expected2 = { 1, 2, 3, 5 };
    assert(list2 == expected2);
    assert(topo.to_cpuset_string(list2) == "1-3,5");

    std::cout << "[PASS] test_cpu_list_parsing\n";
}

void test_all_eleven_fixtures(const fs::path& fixtures_base) {
    using namespace zenith;

    // 1. Ryzen 7950X3D
    {
        MockTopologyFs fs(fixtures_base / "ryzen_7950x3d");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 32);
        assert(topo.has_vcache() == true);
        assert(topo.has_amd_x3d_driver() == true);

        auto cache_cores = topo.get_gaming_cores(CacheSelectionMode::Cache);
        assert(cache_cores.size() == 16);
        assert(topo.to_cpuset_string(cache_cores) == "0-7,16-23");

        auto freq_cores = topo.get_gaming_cores(CacheSelectionMode::Frequency);
        assert(freq_cores.size() == 16);
        assert(topo.to_cpuset_string(freq_cores) == "8-15,24-31");

        assert(topo.form_factor() == FormFactor::Desktop);
        assert(topo.is_energy_conscious() == false);
        std::cout << "  [PASS] fixture: ryzen_7950x3d\n";
    }

    // 2. Ryzen 9950X3D
    {
        MockTopologyFs fs(fixtures_base / "ryzen_9950x3d");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 32);
        assert(topo.has_vcache() == true);
        assert(topo.has_amd_x3d_driver() == true);
        auto cache_cores = topo.get_gaming_cores(CacheSelectionMode::Cache);
        assert(topo.to_cpuset_string(cache_cores) == "0-7,16-23");
        std::cout << "  [PASS] fixture: ryzen_9950x3d\n";
    }

    // 3. Ryzen 7800X3D
    {
        MockTopologyFs fs(fixtures_base / "ryzen_7800x3d");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 16);
        assert(topo.has_vcache() == true);
        assert(topo.get_gaming_cores().size() == 16);
        std::cout << "  [PASS] fixture: ryzen_7800x3d\n";
    }

    // 4. Ryzen 5800X3D
    {
        MockTopologyFs fs(fixtures_base / "ryzen_5800x3d");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 16);
        assert(topo.has_vcache() == true);
        assert(topo.get_gaming_cores().size() == 16);
        std::cout << "  [PASS] fixture: ryzen_5800x3d\n";
    }

    // 5. Ryzen 5600
    {
        MockTopologyFs fs(fixtures_base / "ryzen_5600");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 12);
        assert(topo.has_vcache() == false);
        assert(topo.has_hybrid() == false);
        assert(topo.get_gaming_cores().size() == 12);
        std::cout << "  [PASS] fixture: ryzen_5600\n";
    }

    // 6. Intel Core i9-13900K
    {
        MockTopologyFs fs(fixtures_base / "intel_13900k");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 32);
        assert(topo.has_hybrid() == true);
        auto gaming_cores = topo.get_gaming_cores();
        assert(gaming_cores.size() == 16);
        assert(topo.to_cpuset_string(gaming_cores) == "0-15"); // P-cores

        auto bg_cores = topo.get_background_cores();
        assert(bg_cores.size() == 16);
        assert(topo.to_cpuset_string(bg_cores) == "16-31"); // E-cores
        std::cout << "  [PASS] fixture: intel_13900k\n";
    }

    // 7. Intel Core i7-12700K
    {
        MockTopologyFs fs(fixtures_base / "intel_12700k");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 20);
        assert(topo.has_hybrid() == true);
        assert(topo.to_cpuset_string(topo.get_gaming_cores()) == "0-15");
        assert(topo.to_cpuset_string(topo.get_background_cores()) == "16-19");
        std::cout << "  [PASS] fixture: intel_12700k\n";
    }

    // 8. Intel Core Ultra 9
    {
        MockTopologyFs fs(fixtures_base / "intel_core_ultra_9");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 22);
        assert(topo.has_hybrid() == true);
        assert(topo.to_cpuset_string(topo.get_gaming_cores()) == "0-11"); // P-cores
        std::cout << "  [PASS] fixture: intel_core_ultra_9\n";
    }

    // 9. EPYC 2xNUMA
    {
        MockTopologyFs fs(fixtures_base / "epyc_2x_numa");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 32);
        assert(topo.num_sockets() == 2);
        assert(topo.num_numa_nodes() == 2);
        std::cout << "  [PASS] fixture: epyc_2x_numa\n";
    }

    // 10. VM 4 vCPU
    {
        MockTopologyFs fs(fixtures_base / "vm_4vcpu");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 4);
        assert(topo.is_vm() == true);
        assert(topo.form_factor() == FormFactor::VirtualMachine);
        std::cout << "  [PASS] fixture: vm_4vcpu\n";
    }

    // 11. Steam Deck
    {
        MockTopologyFs fs(fixtures_base / "steam_deck");
        auto topo = CpuTopology::detect(&fs);
        assert(topo.total_cores() == 8);
        assert(topo.form_factor() == FormFactor::Handheld);
        assert(topo.is_on_battery() == true);
        assert(topo.is_energy_conscious() == true);
        std::cout << "  [PASS] fixture: steam_deck\n";
    }
}

void test_hotplug(const fs::path& fixtures_base) {
    using namespace zenith;

    fs::path f = fixtures_base / "ryzen_5600";
    MockTopologyFs mock_fs(f);

    auto topo = CpuTopology::detect(&mock_fs);
    assert(topo.online_cores_count() == 12);

    // Simulate offlining core 5
    fs::path cpu5_online = f / "sys/devices/system/cpu/cpu5/online";
    write_file_string(cpu5_online, "0");

    bool updated = topo.recompute_hotplug(&mock_fs);
    assert(updated == true);
    assert(topo.online_cores_count() == 11);

    // Verify core 5 is no longer in gaming cores
    auto gaming = topo.get_gaming_cores();
    for (int id : gaming) {
        assert(id != 5);
    }

    // Restore online
    write_file_string(cpu5_online, "1");
    topo.recompute_hotplug(&mock_fs);
    assert(topo.online_cores_count() == 12);

    std::cout << "[PASS] test_hotplug\n";
}

int main() {
    std::cout << "Running Stage 4 CPU Topology & Fixtures Verification Tests...\n";

    fs::path fixtures_base = "tests/fixtures";
    zenith::test::generate_all_fixtures(fixtures_base);

    test_cpu_list_parsing();
    test_all_eleven_fixtures(fixtures_base);
    test_hotplug(fixtures_base);

    std::cout << "All Stage 4 CPU Topology Tests Passed Successfully!\n";
    return 0;
}
