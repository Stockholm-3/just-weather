#!/usr/bin/env python3
"""
Generate city datasets from SimpleMaps worldcities.csv
Creates hot_cities.json and all_cities.json for autocomplete
"""

import csv
import json
import sys

def main():
    input_csv = "/home/oleksandr/Documents/proj/just-weather/cache/worldcities.csv"
    hot_cities_output = "/home/oleksandr/Documents/proj/just-weather/data/hot_cities.json"
    all_cities_output = "/home/oleksandr/Documents/proj/just-weather/data/all_cities.json"

    # Demo mode: 100 hot cities, 500 all cities
    HOT_CITIES_COUNT = 500
    ALL_CITIES_COUNT = 48000

    print(f"Reading from: {input_csv}")

    cities = []

    with open(input_csv, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)

        for row in reader:
            # Parse population (may be empty)
            try:
                population = int(float(row['population'])) if row['population'] else 0
            except (ValueError, KeyError):
                population = 0

            # Parse coordinates
            try:
                lat = float(row['lat'])
                lng = float(row['lng'])
            except (ValueError, KeyError):
                continue  # Skip cities without valid coordinates

            city = {
                "name": row['city'],
                "country": row['country'],
                "country_code": row['iso2'],
                "lat": lat,
                "lon": lng,
                "population": population
            }

            cities.append(city)

    print(f"Loaded {len(cities)} cities from CSV")

    # Sort by population (descending)
    cities.sort(key=lambda x: x['population'], reverse=True)

    # Create hot_cities (top N)
    hot_cities = cities[:HOT_CITIES_COUNT]
    hot_cities_data = {"cities": hot_cities}

    with open(hot_cities_output, 'w', encoding='utf-8') as f:
        json.dump(hot_cities_data, f, indent=2, ensure_ascii=False)

    print(f"Created {hot_cities_output} with {len(hot_cities)} cities")

    # Create all_cities (top N * 5)
    all_cities = cities[:ALL_CITIES_COUNT]
    all_cities_data = {"cities": all_cities}

    with open(all_cities_output, 'w', encoding='utf-8') as f:
        json.dump(all_cities_data, f, indent=2, ensure_ascii=False)

    print(f"Created {all_cities_output} with {len(all_cities)} cities")

    # Show top 10 cities
    print("\nTop 10 cities by population:")
    for i, city in enumerate(cities[:10], 1):
        pop_m = city['population'] / 1_000_000
        print(f"{i:2d}. {city['name']:<20} {city['country']:<20} {pop_m:>6.1f}M")

if __name__ == "__main__":
    main()
