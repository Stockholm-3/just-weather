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

## 🧱 Architecture

Client (HTTP or TCP)
↓
Just Weather Server
↓
Weather Server Instance
↓
Open-Meteo API


The **Weather Server Instance** handles the actual communication with the external API and returns parsed JSON data to either HTTP or TCP clients.

---

## 🧩 Project Structure

.

├── Makefile

├── LICENSE

├── src/

│ ├── lib/

│ │ ├── http_server_connection.[ch]

│ │ ├── tcp_server.[ch]

│ │ ├── tcp_client.[ch]

│ │ ├── linked_list.[ch]

│ │ ├── smw.[ch]

│ │ └── utils.h

│ └── server/

│ ├── main.c

│ ├── weather_server.[ch]

│ ├── weather_server_instance.[ch]

│ └── ...

└── lib/

└── jansson/ # JSON parsing library (linked)



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
git clone https://github.com/Stockholm-3/just-weather.git
cd etherskies
```

2. Ensure the lib branch is cloned into ../lib:

To clone the jansson library from project root run:
```bash
git clone --branch lib --single-branch https://github.com/timackevald/etherskies.git ../lib
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

## HTTP API

Base URL: http://<host>:8080

Endpoints

Method	Endpoint	                        Description
GET	    /health	                            Returns server status
GET	    /weather?lat=<float>&lon=<float>    Returns current weather data for coordinates

Example:
```bash
curl "http://localhost:8080/weather?lat=59.33&lon=18.07"
```

Response:
```bash
{
  "source": "open-meteo",
  "coords": { "lat": 59.33, "lon": 18.07 },
  "current": {
    "temperature_c": 8.5,
    "humidity": 75,
    "wind_mps": 3.2
  },
  "updated_at": "2025-11-04T08:00:00Z"
}
```

TCP Protocol

Default port: 9000

Example session (using netcat):
```bash
$ nc localhost 9000
PING
→ PONG

GET WEATHER 59.33 18.07
→ {"temperature_c":8.5,"wind_mps":3.2,"humidity":75}

Error codes:
ERR 100 BAD_REQUEST
ERR 404 NOT_FOUND
ERR 502 UPSTREAM_ERROR
ERR 500 INTERNAL
```

## Design Notes

- HTTP server implemented manually via sockets in http_server_connection.c

- TCP server supports multiple clients using select()
 
- Weather Server Instance performs all Open-Meteo fetches and JSON parsing

- Linked list used for handling concurrent request structures

- libcurl handles outbound HTTP calls to the Open-Meteo API

## 🧭 Endpoint: Get Current Weather by Coordinates

### **GET** `/v1/current/{lat}/{long}`

Retrieve the **current weather data** for the specified geographic coordinates.

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
