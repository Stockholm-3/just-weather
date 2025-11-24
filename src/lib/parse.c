#include "parse.h"

#include <stddef.h>
#include <string.h>

static int copy_bounded(char* dst, size_t dstsz, const char* src, size_t len) {
    if (len + 1 > dstsz)
        return -1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return 0;
}

int parse_string(const char* input, Parser* out) {
    if (!input || !out)
        return -1;

    // ищем "://"
    const char* scheme_sep = strstr(input, "://");
    if (!scheme_sep)
        return -2;

    // protocol
    size_t proto_len = (size_t)(scheme_sep - input);
    if (proto_len == 0 || proto_len >= sizeof(out->protocol))
        return -3;
    if (copy_bounded(out->protocol, sizeof(out->protocol), input, proto_len) !=
        0)
        return -3;

    // после "://"
    const char* p = scheme_sep + 3;

    // домен = до первого '/' или конец строки
    const char* slash      = strchr(p, '/');
    const char* domain_end = slash ? slash : (p + strlen(p));
    size_t      domain_len = (size_t)(domain_end - p);
    if (domain_len == 0 || domain_len >= sizeof(out->domain))
        return -4;
    if (copy_bounded(out->domain, sizeof(out->domain), p, domain_len) != 0)
        return -4;

    // path = с первого '/' и дальше, либо "/" если его не было
    if (slash) {
        size_t path_len = strlen(slash);
        if (path_len + 1 > sizeof(out->path))
            return -5;
        memcpy(out->path, slash, path_len + 1);
    } else {
        if (copy_bounded(out->path, sizeof(out->path), "/", 1) != 0)
            return -5;
    }

    return 0;
}
