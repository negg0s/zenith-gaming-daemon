#include "zenith/cpu_topology.hpp"
#include <cassert>
#include <iostream>

void test_mock_topology() {
    auto topo = zenith::CpuTopology::detect("/non_existent_sysfs_path");
    assert(topo.total_cores() == 8);
    assert(topo.has_vcache() == true);
    assert(topo.get_gaming_cores().size() == 4);
    assert(topo.get_background_cores().size() == 4);

    std::string cpuset = topo.to_cpuset_string(topo.get_gaming_cores());
    assert(cpuset == "0,1,2,3");

    std::cout << "[PASS] test_mock_topology\n";
}

int main() {
    test_mock_topology();
    return 0;
}
