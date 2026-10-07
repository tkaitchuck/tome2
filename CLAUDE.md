# Contributor / reviewer checklist

See `ROBUSTNESS_PLAN.md` for the rationale.

- No new raw C arrays indexed by game IDs without a bounds check
  (prefer `std::array`/`std::vector`, `.at()` where cheap).
- No new `sprintf`/`strcpy`/`strcat`; use `fmt::format` (or `snprintf`).
- No new raw `new`/`delete`; use `std::unique_ptr` / containers.
- Use `ASSERT()` from `tome/assert.hpp` (active in Release, unlike
  `assert()`) for invariants at module boundaries.
- Every bug fix ships with a regression test under `tests/`.
- Add files you touch to `WERROR_SOURCES` in `src/CMakeLists.txt`.
- Run `ctest` from a Debug build (ASAN/UBSAN) before sending a PR.
