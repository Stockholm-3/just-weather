#ifndef __HTTP_CLIENT_H_
#define __HTTP_CLIENT_H_

#include "smw.h"
#include "tcp_client.h"

#ifndef http_client_max_url_length
#    define http_client_max_url_length 1024
#endif

typedef enum {
    http_client_state_init    = 0,
    http_client_state_connect = 1,
    http_client_state_writing = 2,
    http_client_state_reading = 3, // kanske lägger till connecting
    http_client_state_done    = 4,
    http_client_state_dispose = 5,

} http_client_state;

typedef struct {
    http_client_state state;
    SmwTask*          task;
    char              url[http_client_max_url_length + 1];
    uint64_t          timeout;

    void (*callback)(const char* _Event, const char* _Response);

    uint64_t timer;

    uint8_t* write_buffer;
    size_t   write_size;
    size_t   write_offset;

    uint8_t* read_buffer;      // Buffer for incoming data
    size_t   read_buffer_size; // Current size of read buffer
    size_t   body_start;       // Position where HTTP body starts
    size_t   content_len;      // Content-Length from headers
    int      status_code;      // HTTP status code (200, 404, etc.)
    uint8_t* body;             // Extracted response body

    TCPClient*
         tcp_conn; // Handle to TCP connection, är en tcp connection struct
    char hostname[256]; // Parsed from URL
    char path[512];     // Parsed from URL
    int  port;          // Parsed from URL
    char response[8192];
} http_client;

http_client_state http_client_work_init(http_client* _Client);
http_client_state http_client_work_connect(http_client* _Client);
http_client_state http_client_work_writing(http_client* _Client);
http_client_state
http_client_work_reading(http_client* _Client); // THIS WAS MISSING
http_client_state http_client_work_done(http_client* _Client);

int http_client_get(const char* _URL, uint64_t _Timeout,
                    void (*_Callback)(const char* _Event,
                                      const char* _Response));

#endif //__http_client_h_