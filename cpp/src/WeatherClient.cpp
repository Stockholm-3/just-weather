#include "WeatherClient.hpp"

WeatherClient::WeatherClient() {
    // Constructor implementation
}

WeatherClient::~WeatherClient() {
    // Destructor implementation
}

bool WeatherClient::fetchWeatherData(const std::string& location) {
    // Use a fixed test endpoint for simplicity
    std::string path = "/v1/current?lat=59.33&lon=18.07";

    // Use HttpClient to make the request
    HttpClient  client("localhost", 10680);  // Using local server on port 10680
    std::string resp = client.request(path); // Make the GET request
    // Check for empty response indicating failure
    if (resp.empty()) {
        lastResponse_.clear();
        return false;
    }
    // Store the raw response
    lastResponse_ = resp;
    return true;
}

// Return raw HTTP response from last fetch (headers+body)
const std::string& WeatherClient::getRawResponse() const {
    return lastResponse_;
}