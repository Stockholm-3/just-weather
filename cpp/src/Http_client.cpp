#include "Http_client.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Helper function to parse URL into host, port, and path
static void parse_url(const std::string& url, std::string& host,
                      std::string& port, std::string& path) {
    host.clear();
    port.clear();
    path = "/";

    std::string u = url;
    // Strip scheme if present
    const std::string http = "http://";
    if (u.find(http) == 0) {
        u = u.substr(http.size());
    }

    // Find first slash
    size_t      s        = u.find('/');
    std::string hostport = (s == std::string::npos) ? u : u.substr(0, s);
    if (s != std::string::npos)
        path = u.substr(s);

    // Split host and port
    size_t colon = hostport.find(':');
    if (colon != std::string::npos) {
        host = hostport.substr(0, colon);
        port = hostport.substr(colon + 1);
    } else {
        host = hostport;
        port = "80";
    }
}

HttpClient::HttpClient() {}

HttpClient::~HttpClient() {}

bool HttpClient::sendRequest(
    const std::string& url) { // Send HTTP request to the specified URL
    response_.clear();

    // Parse URL into host, port, path
    std::string host, port, path;
    parse_url(url, host, port, path);

    // Resolve hostname to IP address
    struct addrinfo  hints;
    struct addrinfo* res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host.c_str(), port.c_str(), &hints,
                          &res); // Resolve hostname, service, and protocol
    if (gai != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(gai)
                  << std::endl; // Error handling for DNS resolution failure
        return false;
    }

    int sock = -1;
    // Attempt to create and connect socket using resolved addresses
    struct addrinfo* rp;
    for (rp = res; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1)
            continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sock);
        sock = -1;
    }

    // Clean up address info
    freeaddrinfo(res);
    if (sock == -1) {
        std::cerr << "Could not connect to " << host << ":" << port
                  << std::endl;
        return false;
    }

    // Construct HTTP GET request
    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "Connection: close\r\n";
    req << "User-Agent: just-weather-cpp/1.0\r\n";
    req << "Accept: */*\r\n";
    req << "\r\n";

    // Send HTTP request
    std::string reqs   = req.str();
    ssize_t     sent   = 0;
    const char* data   = reqs.c_str();
    size_t      tosend = reqs.size();
    while (tosend > 0) {
        ssize_t n = send(sock, data + sent, tosend, 0);
        if (n < 0) {
            std::cerr << "send error: " << strerror(errno) << std::endl;
            close(sock);
            return false;
        }
        sent += n;
        tosend -= n;
    }

    // Receive HTTP response
    const size_t BUF = 4096;
    char         buffer[BUF];
    ssize_t      r;
    while ((r = recv(sock, buffer, BUF, 0)) > 0) {
        response_.append(buffer, buffer + r);
    }

    if (r < 0) {
        std::cerr << "recv error: " << strerror(errno) << std::endl;
        close(sock);
        return false;
    }

    close(sock);

    // parse status code
    size_t pos = response_.find("\r\n");
    if (pos == std::string::npos)
        return false;
    std::string status = response_.substr(0, pos);
    int         code   = 0;
    if (sscanf(status.c_str(), "HTTP/%*s %d", &code) != 1) {
        // try without version
        if (sscanf(status.c_str(), "%d", &code) != 1)
            code = 0;
    }

    return (code >= 200 && code < 300);
}

const std::string& HttpClient::getResponse() const { return response_; }

HttpClient::HttpClient(const std::string& host, uint16_t port)
    : host_(host), port_(port) {}

// Perform a GET to the configured host:port and the given path
// Returns the raw response (headers+body) or empty on error
std::string HttpClient::request(const std::string& path) {
    // Build URL like http://host:port/path
    std::ostringstream u;
    u << "http://" << host_;
    if (port_ != 80)
        u << ":" << port_;
    // ensure path begins with '/'
    if (!path.empty() && path[0] != '/')
        u << "/";
    u << path;

    bool ok = sendRequest(u.str());
    if (!ok)
        return std::string();
    return response_;
}
