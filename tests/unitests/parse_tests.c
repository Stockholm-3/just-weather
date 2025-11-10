#include "tests.h"
#include "../parse.h"
#include <string.h>

TEST(parse_string_protocol_http) {
    const char* input = "http://example.com/path/to/resource";
    Parser out;
    int rc = parse_string(input, &out);
    assert(rc == 0);
    assert(strcmp(out.protocol, "http") == 0);
}

TEST(parse_string_protocol_https) {
    const char* input = "https://example.com/path/to/resource";
    Parser out;
    int rc = parse_string(input, &out);
    assert(rc == 0);
    assert(strcmp(out.protocol, "https") == 0);
}

void parse_string_all(void) {
    RUN_SUB_TEST(parse_string_protocol_http);
    RUN_SUB_TEST(parse_string_protocol_https);
}
