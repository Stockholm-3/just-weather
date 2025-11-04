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

// --- Struct for parsed URL parameters ---
typedef struct {
    double latitude;
    double longitude;
} WeatherParams;

// --- Callback for libcurl ---
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
    mem->memory[mem->size] = 0; // null-terminate

    return realsize;
}

// --- Parse URL parameters ---
int parse_url_parameters(const char *url, WeatherParams *params) {
    const char *q = strchr(url, '?');
    if (!q) return -1; // no query string
    q++; // skip '?'

    char *query = strdup(q);
    if (!query) return -1;

    char *pair = strtok(query, "&");
    while (pair) {
        char *eq = strchr(pair, '=');
        if (eq) {
            *eq = 0;
            const char *key = pair;
            const char *value = eq + 1;

            if (strcmp(key, "latitude") == 0) {
                params->latitude = atof(value);
            } else if (strcmp(key, "longitude") == 0) {
                params->longitude = atof(value);
            }
        }
        pair = strtok(NULL, "&");
    }

    free(query);
    return 0;
}

// --- Download weather data ---
int get_weather_data_from_url(const char *url, struct MemoryBlock *out) {
    CURL *curl;
    CURLcode res;

    out->memory = NULL;
    out->size = 0;

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

    if (!out->memory || out->size == 0) {
        fprintf(stderr, "Downloaded JSON is empty\n");
        return -3;
    }

    return 0;
}

// --- Parse JSON into Meteo struct ---
int parse_weather_json(const char *json_text, Meteo *meteo) {
    if (!json_text || !*json_text) {
        fprintf(stderr, "Empty JSON text!\n");
        return -1;
    }

    json_error_t error;
    json_t *root = json_loads(json_text, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parsing error on line %d: %s\n", error.line, error.text);
        return -2;
    }

    json_t *current_weather = json_object_get(root, "current_weather");
    if (!json_is_object(current_weather)) {
        fprintf(stderr, "Missing 'current_weather' object\n");
        json_decref(root);
        return -3;
    }

    meteo->temperature   = json_number_value(json_object_get(current_weather, "temperature"));
    meteo->windspeed     = json_number_value(json_object_get(current_weather, "windspeed"));
    meteo->winddirection = json_number_value(json_object_get(current_weather, "winddirection"));
    meteo->humidity      = json_number_value(json_object_get(current_weather, "humidity"));
    meteo->weathercode   = json_integer_value(json_object_get(current_weather, "weathercode"));
    strncpy(meteo->time, json_string_value(json_object_get(current_weather, "time")), sizeof(meteo->time) - 1);
    meteo->time[sizeof(meteo->time) - 1] = '\0';

    json_decref(root);
    return 0;
}

// --- Print Meteo as JSON ---
void print_weather_as_json(double latitude, double longitude, Meteo *meteo) {
    json_t *root = json_object();

    // Coordinates
    json_t *coords = json_object();
    json_object_set_new(coords, "lat", json_real(latitude));
    json_object_set_new(coords, "lon", json_real(longitude));
    json_object_set_new(root, "coords", coords);

    // Current weather
    json_t *current = json_object();
    json_object_set_new(current, "temperature_c", json_real(meteo->temperature));
    json_object_set_new(current, "humidity", json_real(meteo->humidity)); // added
    json_object_set_new(current, "wind_mps", json_real(meteo->windspeed / 3.6));
    json_object_set_new(current, "wind_deg", json_real(meteo->winddirection));
    json_object_set_new(root, "current", current);

    // Timestamp
    json_object_set_new(root, "updated_at", json_string(meteo->time));

    char *json_str = json_dumps(root, JSON_INDENT(2));
    if (json_str) {
        printf("%s\n", json_str);
        free(json_str);
    }

    json_decref(root);
}

// --- Test function ---
void test_fetch_and_parse_url() {
    struct MemoryBlock json_data = {0};
    Meteo meteo;

    const char *url = "https://api.open-meteo.com/v1/forecast?latitude=37.7749&longitude=-122.4194&current_weather=true";
    WeatherParams params = {0};
    if (parse_url_parameters(url, &params) != 0) {
        fprintf(stderr, "Failed to parse URL parameters\n");
        return;
    }

    if (get_weather_data_from_url(url, &json_data) == 0) {
        // Temporary file
        FILE *tmp = tmpfile();
        if (tmp) {
            fwrite(json_data.memory, 1, json_data.size, tmp);
            rewind(tmp);
            printf("Saved JSON to a temporary file\n");
            fclose(tmp);
        }

        // Persistent file
        FILE *json_file = fopen("weather.json", "w");
        if (json_file) {
            fwrite(json_data.memory, 1, json_data.size, json_file);
            fclose(json_file);
            printf("Saved JSON to weather.json\n");
        }

        // Parse and print structured JSON
        if (parse_weather_json(json_data.memory, &meteo) == 0) {
            print_weather_as_json(params.latitude, params.longitude, &meteo);
        }

        free(json_data.memory);
    } else {
        fprintf(stderr, "Failed to fetch weather data\n");
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

