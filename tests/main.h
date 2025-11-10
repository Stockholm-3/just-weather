#define TEST(name) static void name(void)

#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("Running %s... \r\n", #name);                                   \
        name();                                                                \
    } while (0)

#define RUN_SUB_TEST(name)                                                     \
    do {                                                                       \
        printf("Running sub-test %s... ", #name);                              \
        name();                                                                \
        printf("OK\n");                                                        \
    } while (0)

#include <assert.h>
