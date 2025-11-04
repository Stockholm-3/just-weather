#include "weather_server_instance.h"

#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//-----------------Internal Functions-----------------

int weather_server_instance_on_request(void* context);

//----------------------------------------------------

int weather_server_instance_initiate(WeatherServerInstance* instance,
                                     HTTPServerConnection*  connection) {
    instance->connection = connection;

    http_server_connection_set_callback(instance->connection, instance,
                                        weather_server_instance_on_request);

    return 0;
}

int weather_server_instance_initiate_ptr(HTTPServerConnection*   connection,
                                         WeatherServerInstance** instance_ptr) {
    if (instance_ptr == NULL) {
        return -1;
    }

    WeatherServerInstance* instance =
        (WeatherServerInstance*)malloc(sizeof(WeatherServerInstance));
    if (instance == NULL) {
        return -2;
    }

    int result = weather_server_instance_initiate(instance, connection);
    if (result != 0) {
        free(instance);
        return result;
    }

    *(instance_ptr) = instance;

    return 0;
}

int weather_server_instance_on_request(void* context) {
    WeatherServerInstance* inst = (WeatherServerInstance*)context;
    HTTPServerConnection*  conn = inst->connection;

    printf("method: %s\n", conn->method);
    printf("url: %s\n", conn->request_path);
    printf("content size:\n%zu\n", conn->content_len);
    printf("body:\n%s\n", conn->body);

    const char* body_to_send =

        //        "<html>"
        //                               "<head><title>Weather
        //                               Server</title></head>"
        //                               "<body>"
        //                               "<h1>Welcome to the Weather
        //                               Server</h1>"
        //                               "<p>This is a simple HTML page served
        //                               by your " "non-blocking HTTP
        //                               server.</p>"
        //                               "</body>"
        //                               "</html>";

        "{\n"
        "  \"location\": {\n"
        "    \"latitude\": 51.5074,\n"
        "    \"longitude\": -0.1278\n"
        "  },\n"
        "  \"temperature_c\": 21.3,\n"
        "  \"humidity_percent\": 62,\n"
        "  \"windspeed_mps\": 5.4\n"
        "}";

    // Construct HTTP response header
    char header[256];
    int  header_len = snprintf(header, sizeof(header),
                               "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/json\r\n"
                                "Content-Length: %zu\r\n"
                                "\r\n",
                               strlen(body_to_send));

    size_t   total_len = header_len + strlen(body_to_send);
    uint8_t* response  = malloc(total_len + 1);
    if (!response) {
        perror("Out of mem");
        return -1;
    }
    memcpy(response, header, header_len);
    strcpy((char*)response + header_len, body_to_send);

    conn->write_buffer = response;
    conn->write_size   = total_len;
    return 0;
}

/*---------------------------------------------------------------------------------------*/

// Callback for libcurl to write downloaded data into MemoryBlock
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBlock *mem = (struct MemoryBlock *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Out of memory while downloading data\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Download weather data from Open-Meteo into a MemoryBlock
int get_weather_data(double _latitude, double _longitude, struct MemoryBlock *out) {
    CURL *curl;
    CURLcode res;

    out->memory = NULL;
    out->size = 0;

    char url[256];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true",
        _latitude, _longitude);

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to init CURL\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "weather-client/1.0");

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(out->memory);
        out->memory = NULL;
        out->size = 0;
        return -2;
    }

    curl_easy_cleanup(curl);
    return 0; // success
}

// --- Print JSON with indentation (uses Jansson) ---
void print_json_formatted(const char *json_text) {
    json_error_t error;
    json_t *root = json_loads(json_text, 0, &error);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON for formatted print: %s\n", error.text);
        return;
    }

    char *formatted = json_dumps(root, JSON_INDENT(2)); // 2-space indentation
    if (formatted) {
        printf("%s\n", formatted);
        free(formatted);
    }

    json_decref(root);
}

// Parse Open-Meteo JSON into Meteo struct
int parse_weather_json(const char *json_text, Meteo *meteo) {
    json_error_t error;
    json_t *root = json_loads(json_text, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parsing error on line %d: %s\n", error.line, error.text);
        return -1;
    }

    json_t *current_weather = json_object_get(root, "current_weather");
    if (!json_is_object(current_weather)) {
        fprintf(stderr, "Missing 'current_weather' object\n");
        json_decref(root);
        return -2;
    }

    json_t *temperature   = json_object_get(current_weather, "temperature");
    json_t *windspeed     = json_object_get(current_weather, "windspeed");
    json_t *winddirection = json_object_get(current_weather, "winddirection");
    json_t *weathercode   = json_object_get(current_weather, "weathercode");
    json_t *time          = json_object_get(current_weather, "time");

    meteo->temperature   = json_number_value(temperature);
    meteo->windspeed     = json_number_value(windspeed);
    meteo->winddirection = json_number_value(winddirection);
    meteo->weathercode   = json_integer_value(weathercode);
    strncpy(meteo->time, json_string_value(time), sizeof(meteo->time) - 1);
    meteo->time[sizeof(meteo->time) - 1] = '\0';

    json_decref(root);
    return 0;
}

// --- Test function ---
void test_fetch_and_parse() {
    struct MemoryBlock json_data = {0};
    Meteo meteo;

    if (get_weather_data(51.5074, -0.1278, &json_data) == 0) {
        // Print the size of the downloaded data
        printf("MemoryBlock size: %zu bytes\n", json_data.size);

        printf("Raw JSON downloaded:\n");
        print_json_formatted(json_data.memory);
        printf("\n");

        if (parse_weather_json(json_data.memory, &meteo) == 0) {
            printf("Parsed Data:\n");
            printf("Temp: %.1f°C\n", meteo.temperature);
            printf("Wind: %.1f km/h (%.0f°)\n", meteo.windspeed, meteo.winddirection);
            printf("Weather Code: %d\n", meteo.weathercode);
            printf("Time: %s\n", meteo.time);
        }

        free(json_data.memory);
    } else {
        fprintf(stderr, "Failed to fetch weather data.\n");
    }
}

/*--------------------------------------------------------------------------------------*/

void weather_server_instance_work(WeatherServerInstance* instance,
                                  uint64_t               mon_time) {}

void weather_server_instance_dispose(WeatherServerInstance* instance) {}

void weather_server_instance_dispose_ptr(WeatherServerInstance** instance_ptr) {
    if (instance_ptr == NULL || *(instance_ptr) == NULL) {
        return;
    }

    weather_server_instance_dispose(*(instance_ptr));
    free(*(instance_ptr));
    *(instance_ptr) = NULL;
}

