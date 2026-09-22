#pragma once

// Minimal, dependency-free test framework so the DSP core can be unit
// tested on the host machine (plain g++/clang, no NDK, no network fetch of
// a third-party test framework) as well as eventually cross-compiled.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace onc::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& currentFailures() {
    static int failures = 0;
    return failures;
}

inline void reportFailure(const char* file, int line, const std::string& message) {
    ++currentFailures();
    std::fprintf(stderr, "  FAILED (%s:%d): %s\n", file, line, message.c_str());
}

inline int runAll() {
    int totalFailedTests = 0;
    for (auto& t : registry()) {
        currentFailures() = 0;
        std::printf("[ RUN  ] %s\n", t.name.c_str());
        t.fn();
        if (currentFailures() == 0) {
            std::printf("[  OK  ] %s\n", t.name.c_str());
        } else {
            std::printf("[ FAIL ] %s (%d assertion(s) failed)\n", t.name.c_str(), currentFailures());
            ++totalFailedTests;
        }
    }
    std::printf("\n%zu test(s) run, %d failed.\n", registry().size(), totalFailedTests);
    return totalFailedTests == 0 ? 0 : 1;
}

} // namespace onc::test

#define ONC_TEST(name)                                                                     \
    static void onc_test_##name();                                                         \
    static onc::test::Registrar onc_registrar_##name(#name, onc_test_##name);               \
    static void onc_test_##name()

#define ONC_CHECK(cond)                                                                    \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            onc::test::reportFailure(__FILE__, __LINE__, "CHECK failed: " #cond);          \
        }                                                                                  \
    } while (0)

#define ONC_CHECK_NEAR(a, b, eps)                                                          \
    do {                                                                                   \
        const double onc_a = (a);                                                          \
        const double onc_b = (b);                                                          \
        const double onc_eps = (eps);                                                      \
        if (!(std::fabs(onc_a - onc_b) <= onc_eps)) {                                      \
            onc::test::reportFailure(__FILE__, __LINE__,                                   \
                "CHECK_NEAR failed: " #a " ~= " #b " (|" + std::to_string(onc_a - onc_b) +  \
                "| > " + std::to_string(onc_eps) + ")");                                    \
        }                                                                                   \
    } while (0)

#define ONC_CHECK_TRUE_MSG(cond, msg)                                                      \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            onc::test::reportFailure(__FILE__, __LINE__, msg);                             \
        }                                                                                   \
    } while (0)
