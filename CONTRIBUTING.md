# Contributing to Zenith Gaming Daemon

Thank you for your interest in contributing to **Zenith**! We welcome bug reports, performance benchmarks, game profile submissions, and code enhancements from developers of all backgrounds.

---

## Code of Conduct
Please review and adhere to our [Code of Conduct](CODE_OF_CONDUCT.md). We are committed to providing a friendly, safe, and welcoming environment for everyone.

---

## How Can I Contribute?

### 1. Adding or Updating Game Profiles
The easiest way to contribute is adding profiles for your favorite games in `profiles/<appid>_<game>.toml`.
See our [Contributor Guide: Adding a Game Profile](docs/ADDING_A_GAME_PROFILE.md).
Always validate with:
```bash
python3 scripts/validate_game_profiles.py
```

### 2. Implementing New Tweaks
Zenith uses a modular, plugin-like architecture for kernel and hardware optimizations.
Read our [Developer Guide: How to Add a Tweak](docs/ADDING_A_TWEAK.md).
Every tweak must be:
- Accompanied by unit tests in `tests/test_tweaks.cpp`.
- Accompanied by an empirical rationale in `docs/TWEAKS.md`.
- Reversible via idempotent rollback.

### 3. Reporting Bugs
When reporting a bug, always attach the output of:
```bash
zenithctl report --redact > zenith_report.txt
```
This anonymizes your username and hostname while providing vital CPU topology, kernel version, and cgroup diagnostics.

---

## Development Environment & Tooling

### Prerequisites
* GCC ≥ 12 or Clang ≥ 16 (C++20 standard required)
* CMake ≥ 3.20 & Ninja or Make
* `libpipewire-0.3-dev` (optional)
* `clang-format` and `cppcheck`

### Building & Running Tests
```bash
# Clone repository
git clone https://github.com/NEGG0/zenith-gaming-daemon.git
cd zenith-gaming-daemon

# Configure with Clang and Sanitizers (ASan + UBSan)
cmake -B build-dev \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"

# Compile with all threads
cmake --build build-dev -j$(nproc)

# Run full test suite
ctest --test-dir build-dev --output-on-failure --verbose
```

### ThreadSanitizer (TSan) Check
Before submitting PRs touching multithreaded logic or event loop callbacks, verify with TSan:
```bash
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j$(nproc)
ctest --test-dir build-tsan --output-on-failure
```

---

## Coding Standards & Architectural Invariants

1. **Zero Raw Pointers / No Naked `new`/`delete`**: Always use RAII (`std::unique_ptr`, `UniqueFd`, `ScopeExitGuard`).
2. **Crash-Only & Zero-Drift**: All kernel or hardware mutations MUST register an undo action in the Write-Ahead Log (`journal.json`).
3. **No `system()`, `popen()`, or Shell Spawning**: External binaries must be executed strictly via `execve` using trusted absolute paths and whitelists.
4. **No Path Traversal**: Use `openat2` or secure directory-relative open checks to prevent symlink traversal outside `/sys` or `/proc`.
5. **No Exceptions on Hot Paths**: Hot execution paths must use `zenith::Result<T, Error>`.
6. **Code Formatting**: Code must conform to `.clang-format` style.

---

## Commit Guidelines
We follow [Conventional Commits](https://www.conventionalcommits.org/):
* `feat(topology): add core detection for Arrow Lake LP-E cores`
* `fix(cgroups): resolve permission failure on rootless delegate`
* `perf(detector): reduce procfs scan interval jitter`
* `docs(tweaks): add benchmark citation for split_lock mitigation`
* `test(journal): add simulated power-cut during mid-apply`
