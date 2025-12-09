#include "WeatherClient.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WeatherClient::WeatherClient() {

}

WeatherClient::~WeatherClient() {
    
}

// Helper function to extract JSON body from HTTP response
static std::string extractJsonBody(const std::string& httpResponse) {
    size_t pos = httpResponse.find("\r\n\r\n");
    if (pos == std::string::npos) {
        return "";
    }
    return httpResponse.substr(pos + 4);
}

// Parse location string to extract coordinates
// Format: "Stockholm,SE" or "59.33,18.07" or just "Stockholm"
static bool parseLocation(const std::string& location, double& lat, double& lon) {
    // Try to parse as coordinates first (lat,lon)
    size_t comma = location.find(',');
    if (comma != std::string::npos) {
        try {
            std::string latStr = location.substr(0, comma);
            std::string lonStr = location.substr(comma + 1);
            
            // Remove any non-numeric characters except digits, dot, minus
            auto isCoord = [](const std::string& s) {
                for (char c : s) {
                    if (!isdigit(c) && c != '.' && c != '-' && c != ' ') 
                        return false;
                }
                return true;
            };
            
            if (isCoord(latStr) && isCoord(lonStr)) {
                lat = std::stod(latStr);
                lon = std::stod(lonStr);
                return true;
            }
        } catch (...) {
            // Not coordinates, fall through to hardcoded
        }
    }
    
    // Hardcoded locations (you can expand this)
    if (location.find("Stockholm") != std::string::npos) {
        lat = 59.33;
        lon = 18.07;
        return true;
    } else if (location.find("London") != std::string::npos) {
        lat = 51.51;
        lon = -0.13;
        return true;
    } else if (location.find("New York") != std::string::npos) {
        lat = 40.71;
        lon = -74.01;
        return true;
    }
    
    // Default to Stockholm if unknown
    std::cout << "Unknown location, defaulting to Stockholm" << std::endl;
    lat = 59.33;
    lon = 18.07;
    return false;
}

bool WeatherClient::fetchWeatherData(const std::string& location) {
    // Parse location to get coordinates
    double lat, lon;
    parseLocation(location, lat, lon);
    
    // Build request path with actual coordinates
    std::ostringstream pathStream;
    pathStream << "/v1/current?lat=" << lat << "&lon=" << lon;
    std::string path = pathStream.str();
    
    std::cout << "Fetching weather for " << location 
              << " (lat=" << lat << ", lon=" << lon << ")" << std::endl;

    // Try to connect to the local server
    HttpClient client("localhost", 10680);
    std::string resp = client.request(path);
    
    if (resp.empty()) {
        std::cerr << "Failed to fetch weather data from server" << std::endl;
        lastResponse_.clear();
        return false;
    }
    
    lastResponse_ = resp;
    
    // Extract and parse JSON
    std::string jsonBody = extractJsonBody(resp);
    if (jsonBody.empty()) {
        std::cerr << "Failed to extract JSON body" << std::endl;
        return false;
    }
    
    try {
        // Parse JSON
        json weatherData = json::parse(jsonBody);
        
        // Create data directory if it doesn't exist
        system("mkdir -p data");
        
        // Save to generic file (latest fetch)
        std::ofstream outFile("data/weather_data.json");
        if (outFile.is_open()) {
            outFile << weatherData.dump(4);
            outFile.close();
            std::cout << "✓ Weather data saved to data/weather_data.json" << std::endl;
        } else {
            std::cerr << "Failed to open file for writing" << std::endl;
            return false;
        }
        
        // Save with location-specific filename
        std::string filename = location + "_weather.json";
        for (auto& c : filename) {
            if (c == ',' || c == ' ' || c == '/' || c == '\\') c = '_'; // Sanitize filename
        }
        // Save to location-specific file
        std::ofstream locFile("data/" + filename);
        if (locFile.is_open()) {
            locFile << weatherData.dump(4);
            locFile.close();
            std::cout << "✓ Weather data saved to data/" << filename << std::endl;
        }
        
    } catch (const json::parse_error& e) { // JSON parsing error
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
    
    return true;
}

const std::string& WeatherClient::getRawResponse() const {
    return lastResponse_;
}