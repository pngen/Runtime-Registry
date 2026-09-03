#pragma once
// Minimal deterministic test harness. No external framework.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <runtimeregistry/error.hpp>

namespace rr_test {

inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

inline bool report(bool ok, const char* file, int line, const std::string& msg) {
  ++checks();
  if (!ok) { ++failures(); std::printf("  [FAIL] %s:%d %s\n", file, line, msg.c_str()); }
  return ok;
}

struct TestCase { const char* name; std::function<void()> fn; };

inline std::vector<TestCase>& tests() { static std::vector<TestCase> t; return t; }

template <typename Ex, typename Fn>
bool expect_throws_kind(Fn&& fn, runtimeregistry::ErrorKind kind, const char* expr, const char* kindName) {
  ++checks();
  try { fn(); }
  catch (const Ex& e) { if (e.kind() == kind) return true; ++failures(); std::string w = e.what(); std::printf("  [FAIL] %s threw wrong error kind: %s\n", expr, w.c_str()); return false; }
  catch (...) { ++failures(); std::printf("  [FAIL] %s threw unexpected exception type\n", expr); return false; }
  ++failures(); std::printf("  [FAIL] %s did not throw %s\n", expr, kindName); return false;
}

#define CHECK(cond) rr_test::report(static_cast<bool>(cond), __FILE__, __LINE__, #cond)
#define CHECK_EQ(a, b) rr_test::report((a) == (b), __FILE__, __LINE__, std::string(#a) + " == " + #b)
#define CHECK_MSG(cond, msg) rr_test::report(static_cast<bool>(cond), __FILE__, __LINE__, msg)
#define CHECK_THROWS(expr, Ex, kind) rr_test::expect_throws_kind<Ex>([&]() { expr; }, kind, #expr, #kind)

inline int run_all(const char* suite) {
  std::printf("[%s]\n", suite);
  for (auto& t : tests()) {
    std::printf("  test %s ...\n", t.name);
    int before = failures();
    try { t.fn(); }
    catch (const std::exception& e) { ++failures(); std::printf("  [FAIL] %s threw: %s\n", t.name, e.what()); }
    catch (...) { ++failures(); std::printf("  [FAIL] %s threw unknown exception\n", t.name); }
    if (failures() == before) std::printf("    PASS\n");
  }
  std::printf("  %d checks, %d failures\n", checks(), failures());
  return failures() == 0 ? 0 : 1;
}

#define RR_REGISTER(name) \
  static const int rr_register_##name = [] { rr_test::tests().push_back({#name, []() { name(); }}); return 0; }()

}  // namespace rr_test
