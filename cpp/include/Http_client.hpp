#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <cstdint>
#include <string>

class HttpClient {
  public:
    HttpClient(); // default constructor
    // Construct with host and optional port for convenience; request(path) will
    // use these
    HttpClient(const std::string& host, uint16_t port = 80);
    ~HttpClient(); // Destructor implementation

    // Low-level: send a full URL (e.g. "http://host:port/path"). Returns true
    // for 2xx
    bool sendRequest(const std::string& url);

    // Convenience: perform a GET to the configured host:port and the given
    // path. Returns the raw response (headers+body) or empty on error.
    std::string request(const std::string& path);

    // Last response (headers + body)
    const std::string& getResponse() const;

  private:
    std::string response_; // stores raw response from last request
    std::string host_;
    uint16_t    port_ = 80;
};

#endif // HTTP_CLIENT_HPP