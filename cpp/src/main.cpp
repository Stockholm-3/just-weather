#include "Cache.hpp"
#include "Http_client.hpp"
#include "WeatherClient.hpp"

#include <iostream>
#include <vector>

int main() {
    WeatherClient weatherClient;
    Cache cache;
    
    // List of locations to fetch
    std::vector<std::string> locations = {
        "Stockholm,SE",
        "59.33,18.07",      // Coordinates format
        "London,UK",
        "New York,US"
    };
    
    std::cout << "=== Weather Data Fetcher ===" << std::endl << std::endl;
    
    // Fetch weather for each location
    for (const auto& location : locations) {
        std::cout << "--- Fetching: " << location << " ---" << std::endl;
        
        if (weatherClient.fetchWeatherData(location)) {
            std::string response = weatherClient.getRawResponse();
            
            // Store in cache
            cache.storeData(location, response);
            std::cout << "✓ Data cached for " << location << std::endl;
        } else {
            std::cout << "✗ Failed to fetch weather data for " << location << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    // Demonstrate cache retrieval
    std::cout << "=== Cache Retrieval Test ===" << std::endl;
    std::string testLocation = "Stockholm,SE";
    std::string cachedData = cache.retrieveData(testLocation);
    
    // Check if data was retrieved from cache
    if (!cachedData.empty()) {
        std::cout << "✓ Successfully retrieved cached data for " << testLocation << std::endl;
        std::cout << "  (Data size: " << cachedData.size() << " bytes)" << std::endl;
    } else { // No data found in cache
        std::cout << "✗ No cached data found for " << testLocation << std::endl;
    }

    return 0;
}