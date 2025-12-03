#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void fuzz_http_parser(const uint8_t *data, size_t size) {
    if (size < 4) return;
    
    char method[16], path[128], header[256], body[512];
    
    // Parse method - test buffer boundaries
    size_t i = 0;
    while (i < size && i < 15 && data[i] != ' ') {
        method[i] = data[i];
        i++;
    }
    method[i] = '\0';
    
    // Parse path - vulnerability: path traversal, overflow
    size_t start = i + 1;
    i = 0;
    while (start + i < size && i < 127 && data[start + i] != ' ') {
        path[i] = data[start + i];
        i++;
    }
    path[i] = '\0';
    
    // Parse header line - vulnerability: header injection
    start += i + 1;
    i = 0;
    while (start + i < size && i < 255 && data[start + i] != '\r' && data[start + i] != '\n') {
        header[i] = data[start + i];
        i++;
    }
    header[i] = '\0';
    
    // Parse body - vulnerability: content-length mismatch
    start += i + 4; // skip CRLF CRLF
    if (start < size) {
        size_t body_len = (size - start < 511) ? size - start : 511;
        memcpy(body, data + start, body_len);
        body[body_len] = '\0';
    }
    
    // Process (triggers bugs with malformed input)
    if (strstr(path, "..")) {
        fprintf(stderr, "Path traversal attempt\n");
    }
    if (strchr(header, '\n')) {
        fprintf(stderr, "Header injection attempt\n");
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        fprintf(stderr, "Compile: gcc -fsanitize=address,undefined -g -O1 -o fuzzer %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {
        fprintf(stderr, "Invalid file size\n");
        fclose(file);
        return EXIT_FAILURE;
    }

    uint8_t *data = malloc(size);
    if (!data) {
        perror("Memory allocation failed");
        fclose(file);
        return EXIT_FAILURE;
    }

    if (fread(data, 1, size, file) != (size_t)size) {
        fprintf(stderr, "Read failed\n");
        free(data);
        fclose(file);
        return EXIT_FAILURE;
    }
    fclose(file);

    fuzz_http_parser(data, size);

    free(data);
    return EXIT_SUCCESS;
}