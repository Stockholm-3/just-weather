#include "smw.h"

#ifndef HTTP_CLIENT_MAX_URL_LENGTH
#    define HTTP_CLIENT_MAX_URL_LENGTH
#endif

typedef enum {
    http_client_state_init,
    http_client_state_connect,
    http_client_state_connecting,
    http_client_state_writing,
    http_client_state_reading,
    http_client_state_callback,
    http_client_state_dispose,

} http_client_state;

typedef struct {
    http_client_state state;
    smw_task_t*       task;
    char              url[HTTP_CLIENT_MAX_URL_LENGTH + 1];
} http_client;

int http_client_get(const char* URL);
