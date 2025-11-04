# Just Weather

> A lightweight C weather relay server providing both TCP and HTTP APIs powered by Open-Meteo.

Just Weather is a **network server** built as a school project at **Chas Academy (SUVX25)** by **Team Stockholm 3**.  
It acts as a bridge between clients and [open-meteo.com](https://open-meteo.com), providing real-time weather data via both TCP and HTTP interfaces.

![C](https://img.shields.io/badge/C-%2300599C.svg?style=flat&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
[![Build](https://img.shields.io/badge/build-make-blue.svg)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

---

## ✨ Features

- **Dual-protocol support** — access data via HTTP or TCP  
- **Live weather data** — temperature, humidity, and wind speed  
- **Open-Meteo integration** — no API key required  
- **Efficient modular design** — separate libraries for HTTP, TCP, and JSON  
- **C99-compatible** — portable and minimal dependencies  

---

## ⚙️ Requirements

- Linux / WSL environment  
- GCC (C99 compliant)  
- **libcurl** (for HTTP requests)  
- **jansson** (included as submodule or symlink)  
- `make`

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libcurl4-openssl-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc make libcurl-devel
```

**macOS:**
```bash
brew install curl
```

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
make run
```

Release Build 
```bash
make release
```

Run Server
```bash
make run-server
```

Binaries will be created in:
```bash
build/<mode>/server/just-weather
build/<mode>/client/just-weather
```

## Weather API Documentation

**Base URL:**  
```
http://TOD
```

---

### Endpoints

### 1. Get Current Weather

**Endpoint:**  
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

**Response:**  

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
    "wind_mps": 3.2
    "wind_deg": 133.0
  },
  "updated_at": "2025-11-04T08:00:00Z"
}
```

**Response Fields:**  

| Field                  | Type      | Description                                    |
|------------------------|-----------|------------------------------------------------|
| `coords.lat`           | float     | Latitude of the requested location           |
| `coords.lon`           | float     | Longitude of the requested location          |
| `current.temperature_c`| float     | Current temperature in Celsius               |
| `current.wind_mps`     | float     | Current wind speed in meters per second      |
| `current.wind_deg`     | float     | Wind direction in degrees                    |
| `updated_at`           | string    | ISO 8601 timestamp of the last update        |

## Authors

**Team Stockholm 3**
- Chas Academy, SUVX25
- 2025-11-04

## License

This project is licensed under the MIT License - see the [License](LICENSE) file for details.

## Acknowledgments

- [Open-Meteo API](https://open-meteo.com/) - Free weather API
- [Jansson](https://github.com/akheron/jansson) - JSON parsing library
- [libcurl](https://curl.se/libcurl/) - HTTP client library
- Chas Academy instructor and classmates
