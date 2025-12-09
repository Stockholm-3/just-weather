
#ifndef WEATHER_CLIENT_HPP
#define WEATHER_CLIENT_HPP

#include "Http_client.hpp"

#include <string>

class WeatherClient {
  public:
    WeatherClient();  // Constructor implementation
    ~WeatherClient(); // Destructor implementation
    bool fetchWeatherData(
        const std::string& location); // Fetch weather data for a given location
    // Return raw HTTP response from last fetch (headers+body)
    const std::string& getRawResponse() const;

  private:
    // We'll store the last raw response here; requests are made per-call
    std::string lastResponse_;
};

#endif // WEATHER_CLIENT_HPP
