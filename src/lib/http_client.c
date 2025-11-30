#include "http_client.h"

#include "errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define CHUNK_SIZE 4096
#define PORTSIZE 100

/* Decode HTTP chunked transfer encoding.
 * Returns 0 on success, non-zero on failure.
 * Allocates *out which must be freed by caller.
 */
static int decode_chunked(const uint8_t* in, size_t in_len, char** out,
                          size_t* out_len) {
    if (!in || !out || !out_len)
        return -1;

    size_t pos   = 0;
    size_t alloc = 1024;
    char*  buf   = malloc(alloc);
    if (!buf)
        return -2;
    size_t buf_len = 0;

    while (pos < in_len) {
        /* read chunk size line (hex) */
        size_t line_start = pos;
        while (pos < in_len &&
               !(in[pos] == '\r' && pos + 1 < in_len && in[pos + 1] == '\n')) {
            pos++;
        }

        if (pos >= in_len) {
            free(buf);
            return -3;
        }

        size_t line_len = pos - line_start;
        if (line_len == 0) {
            free(buf);
            return -4;
        }

        /* parse hex size */
        char* hex = malloc(line_len + 1);
        if (!hex) {
            free(buf);
            return -5;
        }
        memcpy(hex, in + line_start, line_len);
        hex[line_len] = '\0';

        char*         endptr     = NULL;
        unsigned long chunk_size = strtoul(hex, &endptr, 16);
        if (endptr == hex) {
            free(hex);
            free(buf);
            return -6;
        }
        free(hex);

        /* advance past CRLF */
        pos += 2; /* skip \r\n */

        if (chunk_size == 0) {
            /* consume trailing CRLF after last chunk if present */
            if (pos + 1 < in_len && in[pos] == '\r' && in[pos + 1] == '\n') {
                pos += 2;
            }
            break; /* done */
        }

        /* ensure we have chunk_size bytes available */
        if (pos + chunk_size > in_len) {
            free(buf);
            return -7;
        }

        /* append chunk data */
        if (buf_len + chunk_size + 1 > alloc) {
            while (buf_len + chunk_size + 1 > alloc)
                alloc *= 2;
            char* nbuf = realloc(buf, alloc);
            if (!nbuf) {
                free(buf);
                return -8;
            }
            buf = nbuf;
        }

        memcpy(buf + buf_len, in + pos, chunk_size);
        buf_len += chunk_size;
        pos += chunk_size;

        /* expect CRLF after chunk data */
        if (pos + 1 >= in_len || in[pos] != '\r' || in[pos + 1] != '\n') {
            free(buf);
            return -9;
        }
        pos += 2;
    }

    /* null-terminate */
    if (buf_len + 1 > alloc) {
        char* nbuf = realloc(buf, buf_len + 1);
        if (!nbuf) {
            free(buf);
            return -10;
        }
        buf = nbuf;
    }
    buf[buf_len] = '\0';

    *out     = buf;
    *out_len = buf_len;
    return 0;
}

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

    http_client* _Client = (http_client*)calloc(1, sizeof(http_client));
    if (_Client == NULL)
        return -3;

    /* ensure all fields start zeroed to avoid undefined state */
    _Client->state = http_client_state_init;

    _Client->task = smw_create_task(_Client, http_client_work);

    _Client->callback = NULL;
    _Client->timer    = 0;

    /* copy url (url buffer already zeroed by calloc) */
    strcpy(_Client->url, _URL);

    /* explicit initialization for clarity */
    _Client->tcp_conn    = NULL;
    _Client->hostname[0] = '\0';
    _Client->path[0]     = '\0';
    _Client->port[0]     = '\0';

    _Client->write_buffer     = NULL;
    _Client->write_size       = 0;
    _Client->write_offset     = 0;
    _Client->read_buffer      = NULL;
    _Client->read_buffer_size = 0;
    _Client->body_start       = 0;
    _Client->content_len      = 0;
    _Client->status_code      = 0;
    _Client->body             = NULL;

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

    // 4. Log what we're about to do
    printf("[HTTP_CLIENT] Connecting to %s:%s%s\n", _Client->hostname,
           _Client->port, _Client->path);
    // Move to connect state
    return http_client_state_connect;
}

http_client_state http_client_work_connect(http_client* _Client) {
    // Allocate TCPClient on heap
    TCPClient* tcp_client = malloc(sizeof(TCPClient));
    if (tcp_client == NULL) {
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Memory allocation failed");
        return http_client_state_dispose;
    }

    // Initialize the TCPClient
    tcp_client->fd = -1;

    // Connect using TCP module
    int result =
        tcp_client_connect(tcp_client, _Client->hostname, _Client->port);

    if (result != 0) {
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Failed to initiate connection");
        free(tcp_client);
        return http_client_state_dispose;
    }

    _Client->tcp_conn = tcp_client;

    return http_client_state_connecting;
}

http_client_state http_client_work_connecting(http_client* _Client) {
    if (_Client->tcp_conn == NULL || _Client->tcp_conn->fd < 0) {
        return http_client_state_dispose;
    }

    int fd = _Client->tcp_conn->fd;

    // Check if connection has completed
    int       error = 0;
    socklen_t len   = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return http_client_state_dispose;
    }

    if (error == 0) {
        // Connection successful!
        return http_client_state_writing;
    } else if (error == EINPROGRESS || error == EALREADY) {
        // Still connecting, try again next tick
        return http_client_state_connecting;
    } else {
        // Connection failed
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Connection failed");
        return http_client_state_dispose;
    }
}

http_client_state http_client_work_writing(http_client* _Client) {
    if (_Client->write_buffer == NULL) {
        _Client->write_buffer = malloc(2048);
        if (_Client->write_buffer == NULL) {
            if (_Client->callback != NULL)
                _Client->callback("ERROR", "Memory allocation failed");
            return http_client_state_dispose;
        }

        // browser-like User-Agent та headers
        int len = snprintf(
            (char*)_Client->write_buffer, 2048,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n"
            "Accept: application/json, text/html, application/xml, */*\r\n"
            "Accept-Language: en-US,en;q=0.9\r\n"
            "Accept-Encoding: identity\r\n"
            "Connection: close\r\n"
            "\r\n",
            _Client->path, _Client->hostname);

        _Client->write_size   = len;
        _Client->write_offset = 0;
    }

    // Send data
    ssize_t sent = send(
        _Client->tcp_conn->fd, _Client->write_buffer + _Client->write_offset,
        _Client->write_size - _Client->write_offset, MSG_NOSIGNAL);

    /* write attempt logged at debug level previously; suppressed in normal runs
     */

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return http_client_state_writing; // Try again later
        } else {
            if (_Client->callback != NULL)
                _Client->callback("ERROR", "Send failed");
            return http_client_state_dispose;
        }
    }

    _Client->write_offset += sent;

    if (_Client->write_offset >= _Client->write_size) {
        free(_Client->write_buffer);
        _Client->write_buffer = NULL;
        return http_client_state_reading;
    }

    return http_client_state_writing;
}

http_client_state http_client_work_reading(http_client* client) {
    if (!client) {
        return http_client_state_dispose;
    }

    uint8_t chunk_buffer[CHUNK_SIZE];

    int bytes_read =
        tcp_client_read(client->tcp_conn, chunk_buffer, sizeof(chunk_buffer));

    if (bytes_read < 0) {
        if (client->callback) {
            client->callback("ERROR", "Read failed");
        }
        return http_client_state_dispose;
    } else if (bytes_read == 0) {
        /* No data available right now (non-blocking). Try again later. */
        return http_client_state_reading;
    } else if (bytes_read == -2) {
        /* EOF from peer: treat as end-of-stream */
        if (client->body_start > 0) {
            size_t remaining =
                client->read_buffer_size > client->body_start
                    ? client->read_buffer_size - client->body_start
                    : 0;

            if (client->chunked) {
                /* decode chunked body */
                char*  decoded = NULL;
                size_t dec_len = 0;
                int    rc =
                    decode_chunked(client->read_buffer + client->body_start,
                                   remaining, &decoded, &dec_len);
                if (rc != 0) {
                    if (client->callback)
                        client->callback("ERROR", "Chunked decode failed");
                    return http_client_state_dispose;
                }

                client->body        = (uint8_t*)decoded;
                client->content_len = dec_len;
                return http_client_state_done;
            } else {
                client->content_len = remaining;
                if (client->content_len > 0) {
                    client->body = malloc(client->content_len + 1);
                    if (client->body) {
                        memcpy(client->body,
                               client->read_buffer + client->body_start,
                               client->content_len);
                        client->body[client->content_len] = '\0';
                    }
                }
                return http_client_state_done;
            }
        }

        /* No headers parsed yet but connection closed - nothing to do */
        return http_client_state_dispose;
    }

    /* read progress suppressed in normal logs */

    // Grow buffer
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

    // Parse headers if not done yet
    if (client->body_start == 0) {
        for (int i = 0; i <= client->read_buffer_size - 4; i++) {
            if (client->read_buffer[i] == '\r' &&
                client->read_buffer[i + 1] == '\n' &&
                client->read_buffer[i + 2] == '\r' &&
                client->read_buffer[i + 3] == '\n') {

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

                // Parse status code
                int  status_code     = 0;
                char status_text[64] = {0};
                sscanf(headers, "HTTP/1.%*d %d %63[^\r\n]", &status_code,
                       status_text);

                // Parse Content-Length
                size_t content_len     = 0;
                char*  content_len_ptr = strstr(headers, "Content-Length:");
                if (content_len_ptr) {
                    sscanf(content_len_ptr, "Content-Length: %zu",
                           &content_len);
                }

                // Detect Transfer-Encoding: chunked
                client->chunked =
                    (strstr(headers, "Transfer-Encoding: chunked") != NULL);
                // Detect explicit Connection: close
                client->connection_close =
                    (strstr(headers, "Connection: close") != NULL);

                /* headers parsed */
                free(headers);

                client->status_code = status_code;
                client->content_len =
                    content_len; // 0 means unknown when not present
                client->body_start = header_end;

                break;
            }
        }
    }

    // Check if we have complete response
    if (client->body_start > 0) {
        if (client->content_len > 0) {
            // Known content length -> wait until we have full body
            if (client->read_buffer_size >=
                client->body_start + client->content_len) {
                client->body = malloc(client->content_len + 1);
                if (client->body) {
                    memcpy(client->body,
                           client->read_buffer + client->body_start,
                           client->content_len);
                    client->body[client->content_len] = '\0';
                }
                return http_client_state_done;
            }
        } else if (client->chunked) {
            // Check for terminating chunk sequence "0\r\n\r\n" in buffer
            const char   term[]   = "0\r\n\r\n";
            const size_t term_len = 5; /* actually "0\r\n\r\n" is 5 bytes:
                                          '0','\r','\n','\r','\n' */
            size_t found = SIZE_MAX;
            for (size_t j = client->body_start;
                 j + term_len <= client->read_buffer_size; j++) {
                if (memcmp(client->read_buffer + j, term, term_len) == 0) {
                    found = j;
                    break;
                }
            }

            if (found != SIZE_MAX) {
                size_t total_len = (found + term_len) - client->body_start;
                /* decode chunked data present in buffer */
                char*  decoded = NULL;
                size_t dec_len = 0;
                int    rc =
                    decode_chunked(client->read_buffer + client->body_start,
                                   total_len, &decoded, &dec_len);
                if (rc != 0) {
                    if (client->callback)
                        client->callback("ERROR", "Chunked decode failed");
                    return http_client_state_dispose;
                }
                client->body        = (uint8_t*)decoded;
                client->content_len = dec_len;
                return http_client_state_done;
            }

            return http_client_state_reading;
        } else {
            // No content-length and not chunked -> assume server will close
            // connection before end of body. Use MSG_PEEK to detect EOF on
            // socket.
            int fd = client->tcp_conn ? client->tcp_conn->fd : -1;
            if (fd >= 0) {
                uint8_t peekbuf[1];
                ssize_t p = recv(fd, peekbuf, 1, MSG_PEEK | MSG_DONTWAIT);
                if (p == 0) {
                    // EOF detected: finalize body as all remaining bytes
                    client->content_len =
                        client->read_buffer_size > client->body_start
                            ? client->read_buffer_size - client->body_start
                            : 0;
                    if (client->content_len > 0) {
                        client->body = malloc(client->content_len + 1);
                        if (client->body) {
                            memcpy(client->body,
                                   client->read_buffer + client->body_start,
                                   client->content_len);
                            client->body[client->content_len] = '\0';
                        }
                    }
                    return http_client_state_done;
                } else if (p < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // no data available yet, keep reading
                        return http_client_state_reading;
                    } else {
                        if (client->callback)
                            client->callback("ERROR", "Peek failed");
                        return http_client_state_dispose;
                    }
                }
                // p > 0 -> there is data pending, keep reading
            }
        }
    }

    return http_client_state_reading;
}

http_client_state http_client_work_done(http_client* _Client) {
    if (_Client->callback != NULL) {
        if (_Client->status_code >= 200 && _Client->status_code < 300) {
            // Success response
            _Client->callback("RESPONSE",
                              _Client->body ? (char*)_Client->body : "");
        } else {
            // Error response
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

    switch (_Client->state) {
    case http_client_state_init:
        _Client->state = http_client_work_init(_Client);
        break;

    case http_client_state_connect:
        _Client->state = http_client_work_connect(_Client);
        break;

    case http_client_state_connecting:
        _Client->state = http_client_work_connecting(_Client);
        break;

    case http_client_state_writing:
        _Client->state = http_client_work_writing(_Client);
        break;

    case http_client_state_reading:
        _Client->state = http_client_work_reading(_Client);
        break;

    case http_client_state_done:
        _Client->state = http_client_work_done(_Client);
        break;

    case http_client_state_dispose:
        http_client_dispose(&_Client);
        break;
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
    strcpy(port, "80");
    strcpy(path, "/");

    // Skip "http://" or "https://"
    const char* start = url;
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
        strcpy(port, "80");
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
        strcpy(port, "443");
    }

    // Find the end of hostname
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
        end++;

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