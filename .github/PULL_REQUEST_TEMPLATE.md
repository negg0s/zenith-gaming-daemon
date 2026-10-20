## Description
<!--- Describe your changes in detail -->

## Motivation & Context
<!--- Why is this change required? What problem does it solve? -->
<!--- If it fixes an open issue, please link to the issue here: Fixes # -->

## Type of Change
- [ ] 🐛 Bug fix (non-breaking change which fixes an issue)
- [ ] ⚡ New feature or kernel tweak
- [ ] 🎮 New or updated game profile (`profiles/*.toml`)
- [ ] 📖 Documentation update
- [ ] 🧪 Tests / Benchmarking suite improvement

## Verification Checklist
- [ ] My code follows the repository's coding standards (`.clang-format`).
- [ ] I have verified that all unit tests pass locally (`ctest --test-dir build --output-on-failure`).
- [ ] If adding/editing game profiles, `python3 scripts/validate_game_profiles.py` passes with zero errors.
- [ ] If adding a new tweak, I have included unit tests in `tests/` and documented it in `docs/TWEAKS.md`.
- [ ] Clean under sanitizers (ASan, UBSan, TSan).
- [ ] All mutations implement idempotent rollback to guarantee zero-drift recovery.
