#include "utils.h"

#include <time.h>

uint64_t system_monotonic_ms(void) {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return (uint64_t)spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}
