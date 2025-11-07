# Just Weather

> A lightweight C weather server providing a simple HTTP REST API

Just Weather is a **network server** built as a school project at **Chas Academy (SUVX25)** by **Team Stockholm 3**.  
It acts as a bridge between clients and [open-meteo.com](https://open-meteo.com), providing real-time weather data via a simple REST API

![C](https://img.shields.io/badge/C-%2300599C.svg?style=flat&logo=c&logoColor=white)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://github.com/Stockholm-3/just-weather/actions/workflows/build.yml/badge.svg)](https://github.com/Stockholm-3/just-weather/actions/workflows/build.yml)
[![Check Formatting](https://github.com/Stockholm-3/just-weather/actions/workflows/format-check.yml/badge.svg)](https://github.com/Stockholm-3/just-weather/actions/workflows/format-check.yml)

---

## Features

- **Live weather data** — temperature, weather conditions, and wind speed
- **Open-Meteo integration** — no API key required
- **C99-compatible** — portable and minimal dependencies

---

## Requirements

- Linux / WSL environment
- GCC (C99 compliant)
- **jansson** (included as submodule or symlink)
- `make`

## Installation

1. Clone the repository:
```bash
git clone https://github.com/stockholm-3/just-weather.git
cd etherskies
```

2. Ensure the lib branch is cloned into ../lib:

To clone the jansson library from project root run:
```bash
git clone --branch lib --single-branch https://github.com/stockholm-3/just-weather.git ../lib
```
This will create a lib folder outside of the root with all library source files.

The project uses a symlink to access the Jansson library. The symlink should point to:
```bash
lib/jansson -> ../../lib/jansson
```
More symmlinks may be added in the future.

If the symlink is broken or missing, recreate it from the project root:
```bash
rm lib/jansson
ln -s ../../lib/jansson lib/
```

3. Run the application:
```bash
make run-server
#or
make run-client
```

Binaries will be created in:
```bash
build/<mode>/server/just-weather
build/<mode>/client/just-weather
```

## Weather API Documentation

**Base URL:**  
```
TBD
```

---

### Endpoints

```
GET /current
```

**Description:**  
Retrieves the current weather data for the specified geographic coordinates.

**Query Parameters:**  

| Parameter | Type   | Required | Description                        |
|-----------|--------|----------|------------------------------------|
| `lat`     | float  | Yes      | Latitude of the location (e.g., 59.33) |
| `lon`     | float  | Yes      | Longitude of the location (e.g., 18.07) |

**Example Request:**  
```bash
curl "http://localhost:8080/current?lat=59.33&lon=18.07"
```

**Response Fields:**

| Field                  | Type      | Description                                    |
|------------------------|-----------|------------------------------------------------|
| `coords.lat`           | float     | Latitude of the requested location           |
| `coords.lon`           | float     | Longitude of the requested location          |
| `current.temperature_c`| float     | Current temperature in Celsius               |
| `current.wind_mps`     | float     | Current wind speed in meters per second      |
| `current.wind_deg`     | float     | Wind direction in degrees                    |
| `current.elevation_m`  | float     | Elevation over sea-level in meters           |
| `current.weather_code` | integer   | Condition description (e.g sunny, cloudy etc)|
| `updated_at`           | string    | ISO 8601 timestamp of the last update        |

**Example Response:**  

- **Status Code:** `200 OK`  
- **Content Type:** `application/json`  

```json
{
  "coords": {
    "lat": 59.33,
    "lon": 18.07
  },
  "current": {
    "temperature_c": 8.5,
    "wind_mps": 3.2,
    "wind_deg": 133.0,
    "elevation_m": 45.0,
    "weather_code": 1
  },
  "updated_at": "2025-11-04T08:00:00Z"
}
```

## Authors

**Team Stockholm 3**
- Chas Academy, SUVX25
- 2025-11-04

## License

This project is licensed under the MIT License - see the [License](LICENSE) file for details.

## Acknowledgments

- [Open-Meteo API](https://open-meteo.com/) - Free weather API
- [Jansson](https://github.com/akheron/jansson) - JSON parsing library
- Chas Academy instructor and classmates
