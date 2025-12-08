#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

char* url_encode(const char* str) {
    if (!str) {
        return NULL;
    }

    size_t len         = strlen(str);
    char*  encoded     = malloc(len * 3 + 1);
    size_t encoded_pos = 0;

    if (!encoded) {
        return NULL;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[encoded_pos++] = c;
        } else if (c == ' ') {
            encoded[encoded_pos++] = '+';
        } else {
            sprintf(&encoded[encoded_pos], "%%%02X", c);
            encoded_pos += 3;
        }
    }

    encoded[encoded_pos] = '\0';
    return encoded;
}

int validate_latitude(double lat) { return lat >= -90.0 && lat <= 90.0; }

int validate_longitude(double lon) { return lon >= -180.0 && lon <= 180.0; }

int validate_city_name(const char* city) {
    if (!city || strlen(city) == 0) {
        return 0;
    }
    if (strlen(city) > 100) {
        return 0;
    }
    return 1;
}

uint64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

char* string_trim(char* str) {
    if (!str) {
        return NULL;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    *(end + 1) = '\0';
    return str;
}

char* string_duplicate(const char* str) {
    if (!str) {
        return NULL;
    }
    return strdup(str);
}

void normalize_string_for_cache(const char* in, char* out, size_t out_size) {
    if (!in || !out || out_size == 0) {
        return;
    }

    size_t j            = 0;
    int    prev_was_sep = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == ' ' || c == '\t' || c == '+' || c == '_') {
            if (j == 0 || prev_was_sep) {
                continue;
            }
            out[j++]     = '_';
            prev_was_sep = 1;
        } else {
            if (c >= 'A' && c <= 'Z') {
                out[j++] = (char)(c - 'A' + 'a');
            } else {
                out[j++] = (char)c;
            }
            prev_was_sep = 0;
        }
    }
    if (j > 0 && out[j - 1] == '_') {
        j--;
    }
    out[j] = '\0';
}
