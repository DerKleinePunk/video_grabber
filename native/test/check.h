// Minimal test harness: every check counts, one failure turns the run red.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace check {

inline int& Failures() {
  static int failures = 0;
  return failures;
}

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& Cases() {
  static std::vector<Case> cases;
  return cases;
}

struct Register {
  Register(const char* name, std::function<void()> fn) {
    Cases().push_back({name, std::move(fn)});
  }
};

inline int RunAll() {
  for (const auto& c : Cases()) {
    const int before = Failures();
    c.fn();
    std::printf("%s %s\n", Failures() == before ? "ok  " : "FAIL", c.name);
  }
  std::printf("%zu tests, %d failures\n", Cases().size(), Failures());
  return Failures() == 0 ? 0 : 1;
}

}  // namespace check

#define TEST_CASE(name)                                       \
  static void name();                                         \
  static ::check::Register name##_reg(#name, name);           \
  static void name()

#define EXPECT(cond)                                                     \
  do {                                                               \
    if (!(cond)) {                                                   \
      ++::check::Failures();                                         \
      std::printf("  %s:%d: EXPECT(%s)\n", __FILE__, __LINE__, #cond); \
    }                                                                \
  } while (0)
