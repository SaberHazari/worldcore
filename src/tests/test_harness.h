#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include "utils.h"
#include <stdio.h>

extern int global_tests_run;
extern int global_tests_failed;

#define test_assert(condition) do {                                    \
    global_tests_run++;                                                \
    if(!(condition)) {                                                 \
        global_tests_failed++;                                         \
        printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);  \
    }                                                                  \
} while(0)

#define test_require(condition) do {                                   \
    global_tests_run++;                                                \
    if(!(condition)) {                                                 \
        global_tests_failed++;                                         \
        printf("  FAIL %s:%d  %s (required, test aborted)\n",          \
        __FILE__, __LINE__, #condition);                               \
        return;                                                        \
    }                                                                  \
} while(0)

#define test_assert_eq_u32(a, b) do {                                  \
    global_tests_run++;                                                \
    u32 _a = (a), _b = (b);                                            \
    if(_a != _b) {                                                     \
        global_tests_failed++;                                         \
        printf("  FAIL %s:%d  %s == %s  (%u != %u)\n",                 \
            __FILE__, __LINE__, #a, #b, _a, _b);                       \
    }                                                                  \
} while(0)

#endif // TEST_HARNESS_H