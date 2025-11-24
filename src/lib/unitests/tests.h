#ifndef TESTS_H
#define TESTS_H
#include <assert.h>
#include <stdio.h>

#define TEST(name) static void name(void)

#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("Running %s...\n", #name);                                      \
        name();                                                                \
    } while (0)

#define RUN_SUB_TEST(name)                                                     \
    do {                                                                       \
        printf("  - %s... ", #name);                                           \
        name();                                                                \
        printf("OK\n");                                                        \
    } while (0)

#endif
