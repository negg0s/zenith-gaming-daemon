cmake_minimum_required(VERSION 3.20)
project(zenith-gaming-daemon VERSION 1.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    # Optional Coverage Target
    option(ENABLE_COVERAGE "Enable lcov/gcov code coverage" OFF)
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()

    # Security Hardening Flags
    add_compile_options(
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
        -fstack-protector-strong
        -fstack-clash-protection
        -fcf-protection=full
        -D_FORTIFY_SOURCE=3
    )
    add_link_options(
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
    )
endif()

include_directories(include)

set(ZENITH_SOURCES
    src/config.cpp
    src/cpu_topology.cpp
    src/cgroup_isolator.cpp
    src/sysctl_tuner.cpp
    src/gpu_controller.cpp
    src/process_tuner.cpp
    src/audio_network.cpp
    src/game_detector.cpp
    src/journal.cpp
    src/lock_guard.cpp
    src/safe_mode.cpp
    src/mutation.cpp
    src/engine.cpp
    src/secure_process.cpp
    src/secure_io.cpp
    src/socket_server.cpp
    src/capability_guard.cpp
    src/tweaks/cpu_governor_tweak.cpp
    src/tweaks/cgroup_weight_tweak.cpp
    src/tweaks/uclamp_tweak.cpp
    src/tweaks/timer_slack_tweak.cpp
    src/tweaks/nvme_tweak.cpp
    src/tweaks/usb_autosuspend_tweak.cpp
    src/tweaks/zram_swap_tweak.cpp
    src/tweaks/sysctl_tweak.cpp
    src/tweaks/cstate_latency_tweak.cpp
    src/tweaks/transparent_hugepage_tweak.cpp
    src/tweaks/tweak_registry.cpp
    src/conflict_detector.cpp
    src/per_game_profile.cpp
    src/proc_connector.cpp
    src/background_manager.cpp
    src/irq_tuner.cpp
    src/network_tuner.cpp
    src/pipewire_tuner.cpp
    src/latency_controller.cpp
    src/pressure_monitor.cpp
    src/adaptive_controller.cpp
    src/local_learner.cpp
    src/sched_ext_tuner.cpp
    src/structured_logger.cpp
    src/event_loop.cpp
    src/i18n.cpp
    src/doctor.cpp
    src/tui.cpp
    src/report.cpp
    src/completions.cpp
    src/socket_client.cpp
    src/config_migration.cpp
)

add_library(zenith_core STATIC ${ZENITH_SOURCES})
target_link_libraries(zenith_core PUBLIC ${CMAKE_DL_LIBS})

add_executable(zenith src/main.cpp)
target_link_libraries(zenith PRIVATE zenith_core)

add_executable(zenithctl src/zenithctl/main_ctl.cpp)
target_link_libraries(zenithctl PRIVATE zenith_core)

add_executable(zenith-run src/zenith-run/main_run.cpp)
target_link_libraries(zenith-run PRIVATE zenith_core)

# Installation targets
include(GNUInstallDirs)
install(TARGETS zenith zenithctl zenith-run DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES configs/zenith.conf configs/profiles.conf DESTINATION etc/zenith)
install(FILES systemd/zenith.service DESTINATION lib/systemd/system)
install(FILES man/zenith.8 DESTINATION ${CMAKE_INSTALL_MANDIR}/man8)
install(FILES man/zenithctl.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES man/zenith.conf.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
install(DIRECTORY profiles/ DESTINATION ${CMAKE_INSTALL_DATADIR}/zenith/profiles FILES_MATCHING PATTERN "*.toml")

# Unit testing
enable_testing()

# Ensure assertions in test suites are never compiled out in Release builds
if(NOT MSVC)
    string(REGEX REPLACE "(^| )(-DNDEBUG)( |$)" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REGEX REPLACE "(^| )(-DNDEBUG)( |$)" " " CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

add_executable(test_cpu_topology tests/test_cpu_topology.cpp)
target_link_libraries(test_cpu_topology PRIVATE zenith_core)
add_test(NAME CpuTopologyTest COMMAND test_cpu_topology)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE zenith_core)
add_test(NAME ConfigTest COMMAND test_config)

add_executable(test_sysctl tests/test_sysctl.cpp)
target_link_libraries(test_sysctl PRIVATE zenith_core)
add_test(NAME SysctlTest COMMAND test_sysctl)

add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE zenith_core)
add_test(NAME EngineTest COMMAND test_engine)

add_executable(test_journal tests/test_journal.cpp)
target_link_libraries(test_journal PRIVATE zenith_core)
add_test(NAME JournalTest COMMAND test_journal)

add_executable(test_flock tests/test_flock.cpp)
target_link_libraries(test_flock PRIVATE zenith_core)
add_test(NAME FlockTest COMMAND test_flock)

add_executable(test_safe_mode tests/test_safe_mode.cpp)
target_link_libraries(test_safe_mode PRIVATE zenith_core)
add_test(NAME SafeModeTest COMMAND test_safe_mode)

add_executable(test_fault_injection tests/test_fault_injection.cpp)
target_link_libraries(test_fault_injection PRIVATE zenith_core)
add_test(NAME FaultInjectionMatrixTest COMMAND test_fault_injection)

add_executable(test_security tests/test_security.cpp)
target_link_libraries(test_security PRIVATE zenith_core)
add_test(NAME SecurityTest COMMAND test_security)

add_executable(test_fuzz_corpus tests/test_fuzz_corpus.cpp)
target_link_libraries(test_fuzz_corpus PRIVATE zenith_core)
add_test(NAME FuzzCorpusTest COMMAND test_fuzz_corpus)

add_executable(test_tweaks tests/test_tweaks.cpp)
target_link_libraries(test_tweaks PRIVATE zenith_core)
add_test(NAME TweaksTest COMMAND test_tweaks)

add_executable(test_game_detector tests/test_game_detector.cpp)
target_link_libraries(test_game_detector PRIVATE zenith_core)
add_test(NAME GameDetectorTest COMMAND test_game_detector)

add_executable(test_latency_subsystems tests/test_latency_subsystems.cpp)
target_link_libraries(test_latency_subsystems PRIVATE zenith_core)
add_test(NAME LatencySubsystemsTest COMMAND test_latency_subsystems)

add_executable(test_adaptive_controller tests/test_adaptive_controller.cpp)
target_link_libraries(test_adaptive_controller PRIVATE zenith_core)
add_test(NAME AdaptiveControllerTest COMMAND test_adaptive_controller)

add_executable(test_cpp20_architecture tests/test_cpp20_architecture.cpp)
target_link_libraries(test_cpp20_architecture PRIVATE zenith_core)
add_test(NAME Cpp20ArchitectureTest COMMAND test_cpp20_architecture)

add_executable(zenith_bench_daemon bench/bench_daemon_metrics.cpp)
target_link_libraries(zenith_bench_daemon PRIVATE zenith_core)

add_executable(test_benchmarks tests/test_benchmarks.cpp)
target_link_libraries(test_benchmarks PRIVATE zenith_core)
add_test(NAME BenchmarksTest COMMAND test_benchmarks)

add_executable(test_ux tests/test_ux.cpp)
target_link_libraries(test_ux PRIVATE zenith_core)
add_test(NAME UxTest COMMAND test_ux)

add_executable(test_config_migration tests/test_config_migration.cpp)
target_link_libraries(test_config_migration PRIVATE zenith_core)
add_test(NAME ConfigMigrationTest COMMAND test_config_migration)

# ==============================================================================
# Packaging (CPack: .deb, .rpm, .tar.gz)
# ==============================================================================
set(CPACK_SET_DESTDIR "ON")
set(CPACK_PACKAGE_NAME "zenith-gaming-daemon")
set(CPACK_PACKAGE_VENDOR "Zenith Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ultra-low latency Linux gaming optimizer daemon (C++20)")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 6)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_VERSION "1.6.0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Debian (.deb) configuration
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Emir (NEGG0) <rokmit599@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.35), libstdc++6 (>= 12)")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postrm")

# RPM (.rpm) configuration
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
set(CPACK_RPM_PACKAGE_AUTOREQ "yes")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm/prerm.sh")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm/postinst.sh")

# Generators
set(CPACK_GENERATOR "TGZ;DEB;RPM")
include(CPack)






