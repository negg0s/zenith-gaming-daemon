#include "zenith/common.hpp"
#include "../fuzz/fuzz_targets.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

static std::vector<uint8_t> read_binary_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

void test_corpus_execution() {
    using namespace zenith::fuzz;

    fs::path corpus_base = "fuzz/corpus";
    if (!fs::exists(corpus_base)) {
        corpus_base = "../fuzz/corpus";
    }

    if (fs::exists(corpus_base)) {
        // Config corpus
        if (fs::exists(corpus_base / "config")) {
            for (const auto& entry : fs::directory_iterator(corpus_base / "config")) {
                auto data = read_binary_file(entry.path());
                fuzz_one_config(data.data(), data.size());
            }
        }

        // Sysfs corpus
        if (fs::exists(corpus_base / "sysfs")) {
            for (const auto& entry : fs::directory_iterator(corpus_base / "sysfs")) {
                auto data = read_binary_file(entry.path());
                fuzz_one_sysfs(data.data(), data.size());
            }
        }

        // Socket corpus
        if (fs::exists(corpus_base / "socket")) {
            for (const auto& entry : fs::directory_iterator(corpus_base / "socket")) {
                auto data = read_binary_file(entry.path());
                fuzz_one_socket(data.data(), data.size());
            }
        }

        // Proc corpus
        if (fs::exists(corpus_base / "proc")) {
            for (const auto& entry : fs::directory_iterator(corpus_base / "proc")) {
                auto data = read_binary_file(entry.path());
                fuzz_one_proc(data.data(), data.size());
            }
        }
    }

    std::cout << "[PASS] test_corpus_execution\n";
}

void test_adversarial_inputs() {
    using namespace zenith::fuzz;

    // 1. Empty buffer
    fuzz_one_config(nullptr, 0);
    fuzz_one_sysfs(nullptr, 0);
    fuzz_one_socket(nullptr, 0);
    fuzz_one_proc(nullptr, 0);

    // 2. Single byte patterns
    for (int b = 0; b < 256; ++b) {
        uint8_t byte = static_cast<uint8_t>(b);
        fuzz_one_config(&byte, 1);
        fuzz_one_sysfs(&byte, 1);
        fuzz_one_socket(&byte, 1);
        fuzz_one_proc(&byte, 1);
    }

    // 3. Format strings and edge cases
    std::string malformed = "%s%s%s%s%n%d%x\x00\xff\xfe\x01";
    fuzz_one_config(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size());
    fuzz_one_sysfs(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size());
    fuzz_one_socket(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size());
    fuzz_one_proc(reinterpret_cast<const uint8_t*>(malformed.data()), malformed.size());

    // 4. Broken parentheses for /proc/<pid>/stat parser
    std::vector<std::string> broken_proc_stats = {
        "(", ")", "((", "))", ")(", "1 (steam", "1 steam)", "1 () 2",
        "1 (((nested))) R 1 2 3", "12345 (cmd with \0 in middle) S"
    };
    for (const auto& s : broken_proc_stats) {
        fuzz_one_proc(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    // 5. Giant 128KB payload
    std::vector<uint8_t> giant(128 * 1024, '=');
    fuzz_one_config(giant.data(), giant.size());
    fuzz_one_sysfs(giant.data(), giant.size());
    fuzz_one_socket(giant.data(), giant.size());
    fuzz_one_proc(giant.data(), giant.size());

    std::cout << "[PASS] test_adversarial_inputs\n";
}

int main() {
    std::cout << "Running Stage 2 Fuzz Corpus & Adversarial Regression Tests...\n";
    test_corpus_execution();
    test_adversarial_inputs();
    std::cout << "All Fuzz Regression Tests Passed Successfully!\n";
    return 0;
}
