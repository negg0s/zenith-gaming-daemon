cmake_minimum_required(VERSION 3.20)
project(zenith-gaming-daemon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
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
    src/engine.cpp
)

add_library(zenith_core STATIC ${ZENITH_SOURCES})

add_executable(zenith src/main.cpp)
target_link_libraries(zenith PRIVATE zenith_core)

# Installation targets
include(GNUInstallDirs)
install(TARGETS zenith DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES configs/zenith.conf DESTINATION /etc/zenith)
install(FILES systemd/zenith.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

# Unit testing
enable_testing()

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
