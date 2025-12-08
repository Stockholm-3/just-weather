#include "Cache.hpp"
#include "Http_client.hpp"
#include "WeatherClient.hpp"

#include <iostream>

int main() {
    WeatherClient weatherClient; // Create weather client instance
    Cache         cache;         // Create cache instance
    std::string   location = "Stockholm,SE";

    // Fetch weather data
    if (weatherClient.fetchWeatherData(location)) {
        std::string response = weatherClient.getRawResponse();
        std::cout << "Weather data fetched successfully:\n"
                  << response << std::endl;
        // Store in cache
        cache.storeData(location, response);
    } else {
        std::cout << "Failed to fetch weather data for " << location
                  << std::endl;
    }
    // Retrieve from cache
    std::string cachedData = cache.retrieveData(location);
    if (!cachedData.empty()) {
        std::cout << "Cached data for " << location << ":\n"
                  << cachedData << std::endl;
    } else {
        std::cout << "No cached data for " << location << std::endl;
    }

    return 0;
}
