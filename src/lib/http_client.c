#include "http_client.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CHUNK_SIZE 4096
//---------------Internal functions----------------

void http_client_work(void* _Context, uint64_t _MonTime);
void http_client_dispose(http_client** _ClientPtr);
int parse_url(const char* url, char* hostname, int* port, char* path);
//----------------------------------------------------

int http_client_init(const char* _URL, http_client** _ClientPtr)
{
	if(_URL == NULL || _ClientPtr == NULL)
		return -1;

	if(strlen(_URL) > http_client_max_url_length)
		return -2;

	http_client* _Client = (http_client*)malloc(sizeof(http_client));
	if(_Client == NULL)
		return -3;

	//_Client->state = http_client_state_init;
	_Client->task = smw_create_task(_Client, http_client_work);

	_Client->callback = NULL;
	_Client->timer = 0;

	strcpy(_Client->url, _URL);

	_Client->tcp_conn = NULL;
	_Client->hostname[0] = '\0';
	_Client->path[0] = '\0';
	_Client->port = 80;
	//_Client->response[0] = '\0'; från funtion som finns i client.h

	*(_ClientPtr) = _Client;

	return 0;
}

int http_client_get(const char* _URL, uint64_t _Timeout, void (*_Callback)(const char* _Event, const char* _Response))
{
	http_client* client = NULL;
	if(http_client_init(_URL, &client) != 0)
		return -1;

	client->timeout = _Timeout;
	client->callback = _Callback;

	return 0;
}

http_client_state http_client_work_init(http_client* _Client)
{
	// 1. Parse the URL to extract hostname, port, and path
	if(parse_url(_Client->url, _Client->hostname, &_Client->port, _Client->path) != 0)
	{
		// URL parsing failed
		if(_Client->callback != NULL)
			_Client->callback("ERROR", "Invalid URL");
		return http_client_state_dispose;
	}
	
	// 2. Validate the parsed data
	if(strlen(_Client->hostname) == 0)
	{
		if(_Client->callback != NULL)
			_Client->callback("ERROR", "No hostname in URL");
		return http_client_state_dispose;
	}
	
	// 3. Initialize response buffer
	_Client->response[0] = '\0';
	
	// 4. Log what we're about to do (optional)
	printf("Initializing connection to %s:%d%s\n", 
	       _Client->hostname, _Client->port, _Client->path);
	
	// 5. Move to connect state
	return http_client_state_connect;
}

http_client_state http_client_work_connect(http_client* _Client)
{
    // Parse URL to get hostname and port
    char hostname[256];
    int port = 80;
    char path[512];
    parse_url(_Client->url, hostname, &port, path);
    
    // store tha parsed values in client struct
    strcpy(_Client->hostname, hostname);
    strcpy(_Client->path, path);        
    _Client->port = port;               
    
    // Convert port to string
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    // Allocate TCPClient on heap
    TCPClient* tcp_client = malloc(sizeof(TCPClient));
    if (tcp_client == NULL) {
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Memory allocation failed");
        return http_client_state_dispose;
    }
    
    // CRITICAL: Initialize the TCPClient
    tcp_client->fd = -1;
    
    // Use TCP module to connect!
    int result = tcp_client_connect(tcp_client, hostname, port_str);
    
    if(result != 0) {
        if (_Client->callback != NULL)
            _Client->callback("ERROR", "Failed to initiate connection");
        free(tcp_client);
        return http_client_state_dispose;
    }
    
    // Store the heap-allocated TCP client
    _Client->tcp_conn = tcp_client;
    
    return http_client_state_writing;
}

/*http_client_state http_client_work_connecting(http_client* _Client)//
{ 
    // Check connection progress
    if (_Client->tcp_conn != NULL) {
        // This would depend on your TCP implementation
        if (tcp_is_connected(_Client->tcp_conn)) {
            _Client->isConnected = true;
            return http_client_state_writing;
        }
        
        if (tcp_has_failed(_Client->tcp_conn)) {
            _Client->connection_failed = true;
            if (_Client->callback != NULL)
                _Client->callback("ERROR", "Connection failed");
            return http_client_state_dispose;
        }
    }
    
    // Still connecting
    return http_client_state_connecting;
}*/

http_client_state http_client_work_writing(http_client* _Client)
{
    if(_Client->write_buffer == NULL)
    {
        // Allocate directly as uint8_t*
        _Client->write_buffer = malloc(1024);
        if(_Client->write_buffer == NULL)
        {
            if(_Client->callback != NULL)
                _Client->callback("ERROR", "Memory allocation failed");
            return http_client_state_dispose;
        }
        
        // Cast to char* for snprintf, then back to uint8_t* is automatic
        int len = snprintf((char*)_Client->write_buffer, 1024, 
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 _Client->path, _Client->hostname);
        
        _Client->write_size = len;
        _Client->write_offset = 0;
    }
    
    // Rest of your code remains exactly the same...
    ssize_t sent = tcp_client_write(_Client->tcp_conn,
                           _Client->write_buffer + _Client->write_offset,
                           _Client->write_size - _Client->write_offset);
    
    if(sent > 0)
    {
        _Client->write_offset += sent;
    }
    else if(sent < 0)
    {
        if(_Client->callback != NULL)
            _Client->callback("ERROR", "Send failed");
        free(_Client->write_buffer);
        _Client->write_buffer = NULL;
        return http_client_state_dispose;
    }
    
    if(_Client->write_offset >= _Client->write_size)
    {
        free(_Client->write_buffer);
        _Client->write_buffer = NULL;
        return http_client_state_reading;
    }
    
    return http_client_state_writing;
}

http_client_state http_client_work_reading(http_client* client) {
    if (!client) {
        return -1;
    }

    uint8_t chunk_buffer[CHUNK_SIZE];
    int bytes_read = tcp_client_read(client->tcp_conn, chunk_buffer, sizeof(chunk_buffer));

    if (bytes_read < 0) {
        return -1; // real error
    } else if (bytes_read == 0) {
        return 0;
    }

    // Same buffer growth logic
    size_t new_size = client->read_buffer_size + bytes_read;
    uint8_t* new_buffer = realloc(client->read_buffer, new_size);
    if (!new_buffer) {
        return -1;
    }

    client->read_buffer = new_buffer;
    memcpy(client->read_buffer + client->read_buffer_size, chunk_buffer, bytes_read);
    client->read_buffer_size += bytes_read;

    // Same header parsing logic, just different fields
    if (client->body_start == 0) {
        for (int i = 0; i <= client->read_buffer_size - 4; i++) {
            // Same CRLF CRLF detection
            if (client->read_buffer[i] == '\r' &&
                client->read_buffer[i + 1] == '\n' &&
                client->read_buffer[i + 2] == '\r' &&
                client->read_buffer[i + 3] == '\n') {

                int header_end = i + 4;
                char* headers = malloc(header_end + 1);
                if (!headers) {
                    return -1;
                }

                memcpy(headers, client->read_buffer, header_end);
                headers[header_end] = '\0';

                // Parse response instead of request
                int status_code = 0;
                char status_text[64] = {0};
                sscanf(headers, "HTTP/1.%*d %d %63[^\r\n]", &status_code, status_text);

                // Same Content-Length parsing
                size_t content_len = 0;
                char* content_len_ptr = strstr(headers, "Content-Length:");
                if (content_len_ptr) {
                    sscanf(content_len_ptr, "Content-Length: %zu", &content_len);
                }

                free(headers);

                client->status_code = status_code;
                client->content_len = content_len;
                client->body_start = header_end;

                break;
            }
        }
    }

    // Same completion check logic
    if (client->body_start > 0 && 
        client->read_buffer_size >= client->body_start + client->content_len) {
        
        client->state = http_client_state_done;
        
        // Extract response body
        if (client->content_len > 0) {
            client->body = malloc(client->content_len + 1);
            if (client->body) {
                memcpy(client->body, 
                       client->read_buffer + client->body_start, 
                       client->content_len);
                client->body[client->content_len] = '\0';
            }
        }
        
        // Call client callback
        if (client->callback) {
            char response_info[256];
            snprintf(response_info, sizeof(response_info), 
                    "Status: %d, Body: %s", 
                    client->status_code, 
                    client->body ? (char*)client->body : "");
            client->callback("RESPONSE", response_info);
        }
    }

    return 0;
}


http_client_state http_client_work_done(http_client* _Client)
{
    if (_Client->callback != NULL) {
        // Use the actual response data instead of hardcoded string
        if (_Client->status_code >= 200 && _Client->status_code < 300) {
            // Success response
            _Client->callback("RESPONSE", _Client->body ? (char*)_Client->body : "");
        } else {
            // Error response - include status code
            char error_info[256];
            snprintf(error_info, sizeof(error_info), "HTTP %d: %s", 
                    _Client->status_code, _Client->body ? (char*)_Client->body : "");
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

void http_client_work(void* _Context, uint64_t _MonTime)
{
	http_client* _Client = (http_client*)_Context;

	if(_Client->timer == 0)
	{
		_Client->timer = _MonTime;
	}
	else if(_MonTime >= _Client->timer + _Client->timeout)
	{
		if(_Client->callback != NULL)
			_Client->callback("TIMEOUT", NULL);

		http_client_dispose(&_Client);
		return;
	}

	printf("%i > %s\r\n", _Client->state, _Client->url);

	switch(_Client->state)
	{
		case http_client_state_init:
		{
			_Client->state = http_client_work_init(_Client);
		} break;
		
		case http_client_state_connect:
		{
			_Client->state = http_client_work_connect(_Client);
		} break;
		
		/*case http_client_state_connecting:
		{
			_Client->state = http_client_work_connecting(_Client);
		} break;*/
		
		case http_client_state_writing:
		{
			_Client->state = http_client_work_writing(_Client);
		} break;
		
	    case http_client_state_reading:
		{
			_Client->state = http_client_work_reading(_Client);
		} break;
		
		case http_client_state_done:
		{
			_Client->state = http_client_work_done(_Client);
		} break;
		
		case http_client_state_dispose:
		{
			http_client_dispose(&_Client);
		} break;
		
	}
	
}

void http_client_dispose(http_client** _ClientPtr)
{
	if(_ClientPtr == NULL || *(_ClientPtr) == NULL)
		return;

	http_client* _Client = *(_ClientPtr);

	if(_Client->task != NULL)
		smw_destroy_task(_Client->task);

	free(_Client);

	*(_ClientPtr) = NULL;
}

int parse_url(const char* url, char* hostname, int* port, char* path)
{
    if(url == NULL || hostname == NULL || port == NULL || path == NULL)
        return -1;
    
    // Default values
    *port = 80;
    strcpy(path, "/");  // Default path
    
    // Skip "http://" or "https://"
    const char* start = url;
    if(strncmp(url, "http://", 7) == 0)
    {
        start = url + 7;
        *port = 80;
    }
    else if(strncmp(url, "https://", 8) == 0)
    {
        start = url + 8;
        *port = 443;
    }
    
    // Find the end of hostname (either ':', '/', or end of string)
    const char* end = start;
    while(*end && *end != ':' && *end != '/')
        end++;
    
    // Extract hostname
    int hostname_len = end - start;
    if(hostname_len == 0 || hostname_len > 255)
        return -1;
    
    strncpy(hostname, start, hostname_len);
    hostname[hostname_len] = '\0';
    
    // Check for port
    if(*end == ':')
    {
        end++;  // Skip ':'
        *port = atoi(end);
        
        // Find start of path after port
        while(*end && *end != '/')
            end++;
    }
    
    // Extract path
    if(*end == '/')
    {
        strncpy(path, end, 511);  // Copy the path including the '/'
        path[511] = '\0';
    }
    
    return 0;
}