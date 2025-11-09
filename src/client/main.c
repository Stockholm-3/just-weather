#include <stdio.h>

void custom_callback(const char* response) {
    printf("\n\r------------HERE IS THE "
           "RESPONE-------------\n\r%s\n\r------------END OF "
           "RESPONSE-------------\n\r",
           response);
}

int main(void) { return 0; }
