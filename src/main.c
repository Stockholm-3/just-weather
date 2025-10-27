/*
    Etherskies, a CLI weather-tool.
    This is a school-project from students
    at Chas Academy, SUVX25.
    - Team Stockholm 1
    2025-10
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

int main() {
    cache_global_init();

    add_data_to_cache();
    read_data_from_cache();

    cache_global_destroy();

    return 0;
}
