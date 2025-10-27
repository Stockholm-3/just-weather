# Just Weather

> A CLI weather tool for checking current weather conditions in cities worldwide.

Just Weather is a command-line weather application that fetches real-time weather data from Open-Meteo API. Built as a school project at Chas Academy (SUVX25) by Team Stockholm 3.

![C](https://img.shields.io/badge/c-%2300599C.svg?style=flat&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
[![Check Formatting](https://github.com/timackevald/etherskies/actions/workflows/clang-format.yml/badge.svg)](https://github.com/timackevald/etherskies/actions/workflows/clang-format.yml)

## Features

- **Real-time weather data** - Temperature, wind speed, and humidity
- **Smart caching** - Data cached for 15 minutes to reduce API calls
- **16 Swedish cities** - Pre-configured with major Swedish cities
- **Persistent cache** - Saves city data between sessions
- **Fast lookups** - Doubly-linked list implementation for efficient operations

## Prerequisites

Before building Etherskies, ensure you have the following installed:

- GCC (C99 compatible)
- libcurl development files
- Jansson library (linked via symlink)
- Make

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
cd just-weather
```

2. Ensure the lib branch is cloned into ../lib:

To clone the jansson library from project root run:
```bash
git clone --branch lib --single-branch https://github.com/Stockholm-3/just-weather.git ../lib
```
This will create a lib folder outside of the root with all library source files.

The project uses a symlink to access the Jansson library. The symlink should point to:
```
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

## Usage

When you run Just Weather, you'll see a list of available cities:

```
Stockholm
Göteborg
Malmö
Uppsala
Västerås
...

Select a city: Stockholm

You selected: Stockholm

Current Weather for Stockholm:
Temperature: 8.50 °C
Wind speed: 3.20 m/s
Humidity: 75.00 %
```

Type `q` to exit the application.

## Project Structure

```
just-weather/
├── src/
│   ├── main.c           # Entry point
│   └── libs/
│       ├── city.c       # City list management & caching
│       ├── city.h
│       ├── HTTP.c       # Network operations & JSON parsing
│       ├── HTTP.h
│       ├── meteo.c      # API URL builder
│       ├── meteo.h
│       └── tinydir.h    # Directory traversal (header-only)
├── lib/
│   └── jansson/         # Symlink to external Jansson library
├── includes/
│   └── jansson_config.h # Jansson configuration
├── build/               # Compiled objects and binary
├── cities/              # Cache directory (created at runtime)
├── Makefile
└── README.md
```

## How It Works

### Caching Strategy

Etherskies implements a three-tier caching system:

1. **In-memory cache** - Data stored in the linked list (fastest)
2. **File cache** - JSON files in `./cities/` directory (persistent)
3. **Network fetch** - Only when data is older than 15 minutes

### Data Flow

```
User selects city
    ↓
Check in-memory data (< 15 min old?)
    ↓ No
Check file cache (exists & < 15 min old?)
    ↓ No
Fetch from Open-Meteo API
    ↓
Parse JSON response
    ↓
Update memory & save to file cache
    ↓
Display to user
```

### Bootstrap Cities

On first run, 16 Swedish cities are automatically added:
- Stockholm, Göteborg, Malmö, Uppsala, Västerås
- Örebro, Linköping, Helsingborg, Jönköping, Norrköping
- Lund, Gävle, Sundsvall, Umeå, Luleå, Kiruna

## Building from Source

### Build Options

```bash
make          # Build the project
make run      # Build and run
make clean    # Remove build artifacts
```

### Compiler Flags

- `-std=c99` - C99 standard
- `-Wall -Wextra` - Enable warnings
- `-MMD -MP` - Generate dependency files
- `-g` - Debug symbols

## Dependencies

### External Libraries

- **libcurl** - HTTP requests
- **Jansson** - JSON parsing (linked via symlink to external library)

The Jansson library is accessed via a relative symlink at `lib/jansson`. Ensure the Jansson library is available at `../../jansson/libs/jansson` relative to your project root, or update the symlink to point to your Jansson installation.

## API

This project uses the [Open-Meteo API](https://open-meteo.com/), which is free and doesn't require an API key.

**Example API call:**
```
https://api.open-meteo.com/v1/forecast?latitude=59.33&longitude=18.07&current=temperature_2m,relative_humidity_2m,wind_speed_10m
```

## Authors

**Team Stockholm 3**
- Chas Academy, SUVX25
- 2025-09-10

## License

This project is licensed under the MIT License - see the [License](LICENSE) file for details.

## Acknowledgments

- [Open-Meteo API](https://open-meteo.com/) - Free weather API
- [Jansson](https://github.com/akheron/jansson) - JSON parsing library
- [libcurl](https://curl.se/libcurl/) - HTTP client library
- [tinydir](https://github.com/cxong/tinydir) - Directory traversal library
- Chas Academy instructor and classmates
