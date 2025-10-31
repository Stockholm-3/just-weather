#include "http_client.h"

#include <stdint.h>
#include <stdio.h>

void custom_callback(const char* response) {
    printf("\n\r------------HERE IS THE "
           "RESPONE-------------\n\r%s\n\r------------END OF "
           "RESPONSE-------------\n\r",
           response);
}

int main(void) {
    HttpClient* http_client = NULL;

    if (http_client_init_ptr(&http_client, -1)) {
        perror("failed to create HTTPClient");
        return -1;
    };

    // connect to localhost server
    //     if (HTTPClient_connect(http_client, "127.0.0.1", "8080") != 0) {
    //         perror("Connection failed");
    //         return -1;
    //     }

    if (http_client_connect(http_client, "httpbin.org", "80") != 0) {
        perror("Connection failed");
        return -1;
    }

    // Exemple request body
    const char* body = "{"
                       "\"device\": \"UUID\","
                       "\"time\": \"<time>\","
                       "\"temperature\": \"<temperature>°C\""
                       "}";

    http_client_write(http_client, "/post", "POST", body);

    uint8_t response[4000];
    http_client_read(http_client, response, sizeof(response), custom_callback);

    http_client_dispose_ptr(http_client);
}
