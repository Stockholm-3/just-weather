#include <stdio.h>
#ifdef UNIT_TESTS
  #include "unitests/tests.h"
  void parse_string_all(void);
  void tcp_client_all(void);
#endif

int main(void) {
    printf("Test main executed.\n");
#ifdef UNIT_TESTS
    RUN_TEST(parse_string_all);
    RUN_TEST(tcp_client_all);
#endif
    return 0;
}