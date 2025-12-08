#include "Cache.hpp"

#include <string>

Cache::Cache() {
    // Constructor implementation
}

Cache::~Cache() {
    // Destructor implementation
}

void Cache::storeData(
    const std::string& key,
    const std::string& data) { // Store data in the internal storage
    storage[key] = data;       // Insert or update the key value pair
}

std::string Cache::retrieveData(const std::string& key) {
    auto it = storage.find(key); // Search for the key
    if (it != storage.end()) {   // Key found
        return it->second;       // Return the found data
    }
    return ""; // Return empty string if key not found
}
