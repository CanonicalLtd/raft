/* Define the assert() macro, either as the standard one or the test one. */

#ifndef ASSERT_H_
#define ASSERT_H_

#if defined(RAFT_TEST)
extern void munitErrorfEx(const char *filename,
                          int line,
                          const char *format,
                          ...);
#define assert(expr)                                                        \
    do {                                                                    \
        if (!expr) {                                                        \
            munitErrorfEx(__FILE__, __LINE__, "assertion failed: ", #expr); \
        }                                                                   \
    } while (0)
#elif defined(NDEBUG)
#define assert(x)        \
    do {                 \
        (void)sizeof(x); \
    } while (0)
#else
#include <assert.h>
#endif

#endif /* ASSERT_H_ */
