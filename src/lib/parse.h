#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

typedef struct {
    char protocol[16];
    char domain[256];
    char path[1024];
} Parser;

/* 0 = OK; <0 = error */
int parse_string(const char* input, Parser* out);

#endif /* PARSE_H */
