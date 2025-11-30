#include "http_client.h"

#include "errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 4096
#define PORTSIZE 100
//---------------Internal functions----------------

void http_client_work(void* _Context, uint64_t _MonTime);
void http_client_dispose(http_client** _ClientPtr);
int  parse_url(const char* url, char* hostname, char* port_str, char* path);
//----------------------------------------------------

int http_client_init(const char* _URL, http_client** _ClientPtr,
                     const char* port) {
    if (_URL == NULL || _ClientPtr == NULL)
        return -1;

    if (strlen(_URL) > http_client_max_url_length)
        return -2;

    http_client* _Client = (http_client*)malloc(sizeof(http_client));
    if (_Client == NULL)
        return -3;

    //_Client->state = http_client_state_init;
    _Client->task = smw_create_task(_Client, http_client_work);

    _Client->callback = NULL;
    _Client->timer    = 0;

    strcpy(_Client->url, _URL);

    _Client->tcp_conn    = NULL;
    _Client->hostname[0] = '\0';
    _Client->path[0]     = '\0';
    _Client->port[0]     = '\0';
    //_Client->response[0] = '\0'; från funtion som finns i client.h

    *(_ClientPtr) = _Client;

    return 0;
}

int http_client_get(const char* _URL, uint64_t _Timeout,
                    void (*_Callback)(const char* _Event,
                                      const char* _Response),
                    const char* port) {
    http_client* client = NULL;
    if (http_client_init(_URL, &client, port) != 0)
        return -1;

    client->timeout  = _Timeout;
    client->callback = _Callback;

    return 0;
}

http_client_state http_client_work_init(http_client* _Client) {
    // 1. Parse the URL to extract hostname, port, and path
    if (parse_url(_Client->url, _Client->hostname, _Client->port,
                  _Client->path) != 0) {
        // URL parsing failed
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Invalid URL");
        return http_client_state_dispose;
    }

    // 2. Validate the parsed data
    if (strlen(_Client->hostname) == 0) {
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "No hostname in URL");
        return http_client_state_dispose;
    }

    // 3. Initialize response buffer
    _Client->response[0] = '\0';

    // 4. Log what we're about to do (optional)
    printf("Initializing connection to %s:%s%s\n", _Client->hostname,
           _Client->port, _Client->path);

    // 5. Move to connect state
    return http_client_state_connect;
}

http_client_state http_client_work_connect(http_client* _Client) {
    printf("DEBUG: Starting connection to %s:%s%s\n", _Client->hostname,
           _Client->port, _Client->path);

    // Allocate TCPClient on heap
    TCPClient* tcp_client = malloc(sizeof(TCPClient));
    if (tcp_client == NULL) {
        printf("DEBUG: Memory allocation failed for TCPClient\n");
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Memory allocation failed");
        return http_client_state_dispose;
    }

    // Initialize the TCPClient
    tcp_client->fd = -1; // <-- ADD THIS LINE

    printf("DEBUG: TCPClient allocated and initialized\n");

    // Use TCP module to connect - use _Client->port directly
    printf("DEBUG: Calling tcp_client_connect with %s:%s\n", _Client->hostname,
           _Client->port);
    int result =
        tcp_client_connect(tcp_client, _Client->hostname, _Client->port);
    printf("DEBUG: tcp_client_connect returned: %d\n", result);

    if (result != 0) {
        printf("DEBUG: Connection failed immediately\n");
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Failed to initiate connection");
        free(tcp_client);
        return http_client_state_dispose;
    }

    printf("DEBUG: Connection initiated (non-blocking), moving to connecting "
           "state\n");
    _Client->tcp_conn = tcp_client;

    return http_client_state_connecting;
}

http_client_state http_client_work_connecting(http_client* _Client) {
    if (_Client->tcp_conn == NULL || _Client->tcp_conn->fd < 0) {
        printf("DEBUG: No valid TCP connection\n");
        return http_client_state_dispose;
    }

    int fd = _Client->tcp_conn->fd;

    // Check if connection has completed
    int       error = 0;
    socklen_t len   = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        printf("DEBUG: getsockopt failed: %s\n", strerror(errno));
        return http_client_state_dispose;
    }

    if (error == 0) {
        // Connection successful!
        printf("DEBUG: Connection established! Moving to writing.\n");
        return http_client_state_writing;
    } else if (error == EINPROGRESS || error == EALREADY) {
        // Still connecting, try again next tick
        printf("DEBUG: Still connecting... (error: %s)\n", strerror(error));
        return http_client_state_connecting;
    } else {
        // Connection failed
        printf("DEBUG: Connection failed: %s\n", strerror(error));
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Connection failed");
        return http_client_state_dispose;
    }
}

http_client_state http_client_work_writing(http_client* _Client) {
    if (_Client->write_buffer == NULL) {
        printf("DEBUG: Allocating write buffer\n");
        // Allocate directly as uint8_t*
        _Client->write_buffer = malloc(1024);
        if (_Client->write_buffer == NULL) {
            printf("DEBUG: Memory allocation failed for write buffer\n");
            if (_Client->callback != NULL)
                _Client->callback("ERROR", "Memory allocation failed");
            return http_client_state_dispose;
        }

        // Cast to char* for snprintf, then back to uint8_t* is automatic
        printf("DEBUG: Formatting HTTP request for path='%s', hostname='%s'\n",
               _Client->path, _Client->hostname);
        int len = snprintf((char*)_Client->write_buffer, 1024,
                           "GET %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           _Client->path, _Client->hostname);

        printf("DEBUG: HTTP request formatted, length=%d\n", len);
        printf("DEBUG: Request content: %s\n", (char*)_Client->write_buffer);

        _Client->write_size   = len;
        _Client->write_offset = 0;
    }

    // Add debugging for the send operation
    printf("DEBUG: Attempting to send %zu bytes (offset=%zu, total=%zu)\n",
           _Client->write_size - _Client->write_offset, _Client->write_offset,
           _Client->write_size);

    // Your existing send code here - add error checking
    ssize_t sent = send(
        _Client->tcp_conn->fd, _Client->write_buffer + _Client->write_offset,
        _Client->write_size - _Client->write_offset, MSG_NOSIGNAL);

    printf("DEBUG: send() returned: %zd, errno=%d (%s)\n", sent, errno,
           strerror(errno));

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("DEBUG: Send would block, retrying later\n");
            return http_client_state_writing; // Try again later
        } else {
            printf("DEBUG: Send failed with error: %s\n", strerror(errno));
            if (_Client->callback != NULL)
                _Client->callback("ERROR", "Send failed");
            return http_client_state_dispose;
        }
    }

    printf("DEBUG: Successfully sent %zd bytes\n", sent);
    _Client->write_offset += sent;

    if (_Client->write_offset >= _Client->write_size) {
        printf("DEBUG: All data sent, moving to reading state\n");
        free(_Client->write_buffer);
        _Client->write_buffer = NULL;
        return http_client_state_reading;
    }

    printf("DEBUG: More data to send, remaining=%zu\n",
           _Client->write_size - _Client->write_offset);
    return http_client_state_writing;
}

http_client_state http_client_work_reading(http_client* client) {
    if (!client) {
        return http_client_state_dispose;
    }

    printf("DEBUG: In reading state, read_buffer_size=%zu, body_start=%zu\n",
           client->read_buffer_size, client->body_start);

    uint8_t chunk_buffer[CHUNK_SIZE];
    int     bytes_read =
        tcp_client_read(client->tcp_conn, chunk_buffer, sizeof(chunk_buffer));

    printf("DEBUG: tcp_client_read returned: %d\n", bytes_read);

    if (bytes_read < 0) {
        printf("DEBUG: Read error: %s\n", strerror(errno));
        if (client->callback) {
            client->callback("ERROR", "Read failed");
        }
        return http_client_state_dispose;
    } else if (bytes_read == 0) {
        printf("DEBUG: No data available, would block\n");

        // Simple timeout check - you'll need to implement proper timing
        // For now, just keep waiting
        return http_client_state_reading;
    }

    printf("DEBUG: Received %d bytes\n", bytes_read);

    // Print the raw data as string to see what we're getting
    printf("DEBUG: Raw data received: \"");
    for (int i = 0; i < bytes_read; i++) {
        if (chunk_buffer[i] >= 32 && chunk_buffer[i] <= 126) {
            printf("%c", chunk_buffer[i]);
        } else {
            printf("\\x%02x", chunk_buffer[i]);
        }
    }
    printf("\"\n");

    // Same buffer growth logic
    size_t   new_size   = client->read_buffer_size + bytes_read;
    uint8_t* new_buffer = realloc(client->read_buffer, new_size);
    if (!new_buffer) {
        if (client->callback) {
            client->callback("ERROR", "Memory allocation failed");
        }
        return http_client_state_dispose;
    }

    client->read_buffer = new_buffer;
    memcpy(client->read_buffer + client->read_buffer_size, chunk_buffer,
           bytes_read);
    client->read_buffer_size += bytes_read;

    // Debug: print the entire buffer so far
    if (client->read_buffer_size > 0) {
        printf("DEBUG: Total buffer so far (%zu bytes): \"",
               client->read_buffer_size);
        for (int i = 0;
             i <
             (client->read_buffer_size < 200 ? client->read_buffer_size : 200);
             i++) {
            if (client->read_buffer[i] >= 32 && client->read_buffer[i] <= 126) {
                printf("%c", client->read_buffer[i]);
            } else {
                printf("\\x%02x", client->read_buffer[i]);
            }
        }
        printf("\"\n");
    }

    // Header parsing logic
    if (client->body_start == 0) {
        printf("DEBUG: Looking for end of headers (CRLF CRLF)...\n");
        for (int i = 0; i <= client->read_buffer_size - 4; i++) {
            if (client->read_buffer[i] == '\r' &&
                client->read_buffer[i + 1] == '\n' &&
                client->read_buffer[i + 2] == '\r' &&
                client->read_buffer[i + 3] == '\n') {

                printf("DEBUG: Found end of headers at position %d\n", i);
                int   header_end = i + 4;
                char* headers    = malloc(header_end + 1);
                if (!headers) {
                    if (client->callback) {
                        client->callback("ERROR", "Memory allocation failed");
                    }
                    return http_client_state_dispose;
                }

                memcpy(headers, client->read_buffer, header_end);
                headers[header_end] = '\0';

                printf("DEBUG: Headers:\n%s\n", headers);

                // Parse response
                int  status_code     = 0;
                char status_text[64] = {0};
                int  parsed = sscanf(headers, "HTTP/1.%*d %d %63[^\r\n]",
                                     &status_code, status_text);
                printf("DEBUG: Parsed status: %d, code=%d, text=%s\n", parsed,
                       status_code, status_text);

                // Parse Content-Length
                size_t content_len     = 0;
                char*  content_len_ptr = strstr(headers, "Content-Length:");
                if (content_len_ptr) {
                    sscanf(content_len_ptr, "Content-Length: %zu",
                           &content_len);
                    printf("DEBUG: Found Content-Length: %zu\n", content_len);
                } else {
                    printf("DEBUG: No Content-Length header found\n");
                }

                free(headers);

                client->status_code = status_code;
                client->content_len = content_len;
                client->body_start  = header_end;

                printf("DEBUG: Headers parsed - Status: %d, Content-Length: "
                       "%zu, Body starts at: %zu\n",
                       status_code, content_len, client->body_start);
                break;
            }
        }

        if (client->body_start == 0) {
            printf("DEBUG: Still looking for end of headers...\n");
        }
    }

    // Check if we have a complete response
    if (client->body_start > 0) {
        printf("DEBUG: Headers found, checking completeness - body_start=%zu, "
               "content_len=%zu, total_received=%zu\n",
               client->body_start, client->content_len,
               client->read_buffer_size);

        if (client->read_buffer_size >=
            client->body_start + client->content_len) {
            printf("DEBUG: Complete response received!\n");

            // Extract response body
            if (client->content_len > 0) {
                client->body = malloc(client->content_len + 1);
                if (client->body) {
                    memcpy(client->body,
                           client->read_buffer + client->body_start,
                           client->content_len);
                    client->body[client->content_len] = '\0';
                    printf("DEBUG: Body extracted: %s\n", (char*)client->body);
                }
            }

            // Call client callback
            if (client->callback) {
                char response_info[256];
                snprintf(response_info, sizeof(response_info),
                         "Status: %d, Body: %s", client->status_code,
                         client->body ? (char*)client->body : "");
                client->callback("RESPONSE", response_info);
            }

            return http_client_state_done;
        } else {
            printf("DEBUG: Incomplete body - need %zu more bytes\n",
                   (client->body_start + client->content_len) -
                       client->read_buffer_size);
        }
    }

    return http_client_state_reading;
}

http_client_state http_client_work_done(http_client* _Client) {
    if (_Client->callback != NULL) {
        // Use the actual response data instead of hardcoded string
        if (_Client->status_code >= 200 && _Client->status_code < 300) {
            // Success response
            _Client->callback("RESPONSE",
                              _Client->body ? (char*)_Client->body : "");
        } else {
            // Error response - include status code
            char error_info[256];
            snprintf(error_info, sizeof(error_info), "HTTP %d: %s",
                     _Client->status_code,
                     _Client->body ? (char*)_Client->body : "");
            _Client->callback("ERROR", error_info);
        }
    }

    // Clean up resources
    if (_Client->read_buffer) {
        free(_Client->read_buffer);
        _Client->read_buffer = NULL;
    }

    if (_Client->body) {
        free(_Client->body);
        _Client->body = NULL;
    }

    if (_Client->write_buffer) {
        free(_Client->write_buffer);
        _Client->write_buffer = NULL;
    }

    // Close TCP connection
    if (_Client->tcp_conn) {
        tcp_client_disconnect(_Client->tcp_conn);
        _Client->tcp_conn = NULL;
    }

    return http_client_state_dispose;
}

void http_client_work(void* _Context, uint64_t _MonTime) {
    http_client* _Client = (http_client*)_Context;

    if (_Client->timer == 0) {
        _Client->timer = _MonTime;
    } else if (_MonTime >= _Client->timer + _Client->timeout) {
        if (_Client->callback != NULL)
            _Client->callback("TIMEOUT", NULL);

        http_client_dispose(&_Client);
        return;
    }

    printf("%i > %s\r\n", _Client->state, _Client->url);

    switch (_Client->state) {
    case http_client_state_init: {
        _Client->state = http_client_work_init(_Client);
    } break;

    case http_client_state_connect: {
        _Client->state = http_client_work_connect(_Client);
    } break;

    case http_client_state_connecting: {
        _Client->state = http_client_work_connecting(_Client);
    } break;

    case http_client_state_writing: {
        _Client->state = http_client_work_writing(_Client);
    } break;

    case http_client_state_reading: {
        _Client->state = http_client_work_reading(_Client);
    } break;

    case http_client_state_done: {
        _Client->state = http_client_work_done(_Client);
    } break;

    case http_client_state_dispose: {
        http_client_dispose(&_Client);
    } break;
    }
}

void http_client_dispose(http_client** _ClientPtr) {
    if (_ClientPtr == NULL || *(_ClientPtr) == NULL)
        return;

    http_client* _Client = *(_ClientPtr);

    if (_Client->task != NULL)
        smw_destroy_task(_Client->task);

    free(_Client);

    *(_ClientPtr) = NULL;
}

int parse_url(const char* url, char* hostname, char* port, char* path) {
    if (url == NULL || hostname == NULL || port == NULL || path == NULL)
        return -1;

    // Default values
    strcpy(port, "80"); // Default port as string
    strcpy(path, "/");  // Default path

    // Skip "http://" or "https://"
    const char* start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
        strcpy(port, "80");
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
        strcpy(port, "443");
    }

    // Find the end of hostname (either ':', '/', or end of string)
    const char* end = start;
    while (*end && *end != ':' && *end != '/')
        end++;

    // Extract hostname
    int hostname_len = end - start;
    if (hostname_len == 0 || hostname_len > 255)
        return -1;

    strncpy(hostname, start, hostname_len);
    hostname[hostname_len] = '\0';

    // Check for port
    if (*end == ':') {
        end++; // Skip ':'

        // Extract port number as string
        const char* port_start = end;
        while (*end && *end != '/')
            end++;

        int port_len = end - port_start;
        if (port_len > 0 && port_len < 16) {
            strncpy(port, port_start, port_len);
            port[port_len] = '\0';
        }
    }

    // Extract path
    if (*end == '/') {
        strncpy(path, end, 511);
        path[511] = '\0';
    }

    return 0;
}