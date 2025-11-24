#include <stdio.h>
#ifdef UNIT_TESTS
  #include "unitests/tests.h"
  void parse_string_all(void);
  void tcp_client_all(void);
#endif

#ifdef UNIT_TESTS
int main(void)
{
    printf("Test main executed.\n");

    RUN_TEST(parse_string_all);
    RUN_TEST(tcp_client_all);

    return 0;
}
#endif