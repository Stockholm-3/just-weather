#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

/* URL utilities */
char* url_encode(const char* str);

/* Validation */
int validate_latitude(double lat);
int validate_longitude(double lon);
int validate_city_name(const char* city);

/* Time */
uint64_t get_current_time_ms();

/* String */
char* string_trim(char* str);
char* string_duplicate(const char* str);

/* Cache key normalization */
void normalize_string_for_cache(const char* in, char* out, size_t out_size);

#endif
