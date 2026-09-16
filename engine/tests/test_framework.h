#pragma once

// Minimal single-header test framework: no external dependency, no network
// fetch needed. Each TEST(name) block registers itself; RUN_ALL_TESTS()
// executes them all and reports a pass/fail summary.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& failureCount() {
    static int n = 0;
    return n;
}

inline const char*& currentTest() {
    static const char* t = "";
    return t;
}

} // namespace testfw

#define TEST(name)                                                             \
    static void name();                                                        \
    static testfw::Registrar registrar_##name(#name, name);                    \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "  FAIL [%s] %s:%d: CHECK(%s)\n",             \
                         testfw::currentTest(), __FILE__, __LINE__, #cond);     \
            ++testfw::failureCount();                                          \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        auto va = (a);                                                        \
        auto vb = (b);                                                        \
        if (!(va == vb)) {                                                     \
            std::fprintf(stderr, "  FAIL [%s] %s:%d: CHECK_EQ(%s, %s)\n",      \
                         testfw::currentTest(), __FILE__, __LINE__, #a, #b);    \
            ++testfw::failureCount();                                          \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                   \
    do {                                                                       \
        double va = (double)(a);                                              \
        double vb = (double)(b);                                              \
        double d = va - vb;                                                   \
        if (d < 0) d = -d;                                                    \
        if (d > (eps)) {                                                      \
            std::fprintf(stderr, "  FAIL [%s] %s:%d: CHECK_NEAR(%s, %s) |%f - %f| > %f\n", \
                         testfw::currentTest(), __FILE__, __LINE__, #a, #b, va, vb, (double)(eps)); \
            ++testfw::failureCount();                                          \
        }                                                                      \
    } while (0)

inline int RUN_ALL_TESTS() {
    int total = (int)testfw::registry().size();
    int startFailures = testfw::failureCount();
    for (auto& tc : testfw::registry()) {
        testfw::currentTest() = tc.name.c_str();
        int before = testfw::failureCount();
        tc.fn();
        if (testfw::failureCount() == before) {
            std::printf("  OK   %s\n", tc.name.c_str());
        }
    }
    int failed = testfw::failureCount() - startFailures;
    std::printf("%d test cases, %d failed checks\n", total, failed);
    return failed == 0 ? 0 : 1;
}
