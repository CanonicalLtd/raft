/* Convenience macros to reduce munit boiler plate. */

#ifndef TEST_RUNNER_H_
#define TEST_RUNNER_H_

#include "munit.h"

/* Top-level suites array declaration.
 *
 * These top-level suites hold all module-level child suites and must be defined
 * and then set as child suites of a root suite created at runtime by the test
 * runner's main(). This can be done using the TEST_RUNNER macro. */
extern MunitSuite _main_suites[];
extern int _main_suites_n;

/* Maximum number of test cases for each suite */
#define SUITE_CAP 128

/* Define the top-level suites array and the main() function of the test. */
#define RUNNER(NAME)                                               \
    MunitSuite _main_suites[SUITE_CAP];                            \
    int _main_suites_n = 0;                                        \
                                                                   \
    int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)])    \
    {                                                              \
        MunitSuite suite = {(char *)"", NULL, _main_suites, 1, 0}; \
        return munit_suite_main(&suite, (void *)NAME, argc, argv); \
    }

/* Declare and register a new test suite #S belonging to the file's test module.
 *
 * A test suite is a pair of static variables:
 *
 * static MunitTest _##S##_suites[SUITE_CAP]
 * static MunitTest _##S##Tests[SUITE_CAP]
 *
 * The tests and suites attributes of the next available MunitSuite slot in the
 * _module_suites array will be set to the suite's tests and suites arrays, and
 * the prefix attribute of the slot will be set to /S. */
#define SUITE(S)     \
    SUITE_DECLARE(S) \
    SUITE_ADD_CHILD(main, #S, S)

/* Declare and register a new test. */
#define TEST(S, C, SETUP, TEAR_DOWN, OPTIONS, PARAMS)                \
    static MunitResult test_##S##_##C(const MunitParameter params[], \
                                      void *data);                   \
    TEST_ADD_TO_SUITE(S, C, SETUP, TEAR_DOWN, OPTIONS, PARAMS)       \
    static MunitResult test_##S##_##C(                               \
        MUNIT_UNUSED const MunitParameter params[], MUNIT_UNUSED void *data)

#define SKIP_IF_NO_FIXTURE \
    if (f == NULL) {       \
        return MUNIT_SKIP; \
    }

/* Declare the MunitSuite[] and the MunitTest[] arrays that compose the test
 * suite identified by S. */
#define SUITE_DECLARE(S)                                       \
    static MunitSuite _##S##_suites[SUITE_CAP];                \
    static MunitTest _##S##Tests[SUITE_CAP];                   \
    static MunitTestSetup _##S##_setup = NULL;                 \
    static MunitTestTearDown _##S##_tear_down = NULL;          \
    static int _##S##_suites_n = 0;                            \
    static int _##S##Tests_n = 0;                              \
    __attribute__((constructor)) static void _##S##_init(void) \
    {                                                          \
        memset(_##S##_suites, 0, sizeof(_##S##_suites));       \
        memset(_##S##Tests, 0, sizeof(_##S##Tests));           \
        (void)_##S##_suites_n;                                 \
        (void)_##S##Tests_n;                                   \
        (void)_##S##_setup;                                    \
        (void)_##S##_tear_down;                                \
    }

/* Set the tests and suites attributes of the next available slot of the
 * MunitSuite[] array of S1 to the MunitTest[] and MunitSuite[] arrays of S2,
 * using the given PREXIX. */
#define SUITE_ADD_CHILD(S1, PREFIX, S2)                                \
    __attribute__((constructor)) static void _##S1##_##S2##_init(void) \
    {                                                                  \
        int n = _##S1##_suites_n;                                      \
        _##S1##_suites[n].prefix = PREFIX;                             \
        _##S1##_suites[n].tests = _##S2##Tests;                        \
        _##S1##_suites[n].suites = _##S2##_suites;                     \
        _##S1##_suites[n].iterations = 0;                              \
        _##S1##_suites[n].options = 0;                                 \
        _##S1##_suites_n = n + 1;                                      \
    }

/* Add a test case to the MunitTest[] array of suite S. */
#define TEST_ADD_TO_SUITE(S, C, SETUP, TEAR_DOWN, OPTIONS, PARAMS)            \
    __attribute__((constructor)) static void _##S##Tests_##C##_init(void)     \
    {                                                                         \
        MunitTest *tests = _##S##Tests;                                       \
        int n = _##S##Tests_n;                                                \
        TEST_SET_IN_ARRAY(tests, n, "/" #C, test_##S##_##C, SETUP, TEAR_DOWN, \
                          OPTIONS, PARAMS);                                   \
        _##S##Tests_n = n + 1;                                                \
    }

/* Set the values of the I'th test case slot in the given test array */
#define TEST_SET_IN_ARRAY(TESTS, I, NAME, FUNC, SETUP, TEAR_DOWN, OPTIONS, \
                          PARAMS)                                          \
    TESTS[I].name = NAME;                                                  \
    TESTS[I].test = FUNC;                                                  \
    TESTS[I].setup = SETUP;                                                \
    TESTS[I].tear_down = TEAR_DOWN;                                        \
    TESTS[I].options = OPTIONS;                                            \
    TESTS[I].parameters = PARAMS

#endif /* TEST_RUNNER_H_ */
