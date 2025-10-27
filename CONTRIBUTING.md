C99 Coding Style Guidelines

- Variable naming: my_var
- User-defined types: Something_t
- Structs: struct struct_name
- Typedefs: typedef struct_name typedef_name
- Defines (macros): use uppercase, e.g. #define CAPSLOCK
- Constants: use uppercase, e.g. CAPSLOCK
- Function naming: descriptive names matching h-file/c-file context, e.g. http_my_func
- Pointers: use _ptr suffix, e.g.
    http_mem_buf_t **mem_buf_ptr;
    http_parser_t *parser_ptr;
- Always use curly braces {} even for single-line statements.
- Function return values: use typedef enum codes, e.g. -1, STATUS_FAIL, STATUS_OK, STATUS_EXIT.
- Tabs are preferred over spaces.

We write modular, reusable code and avoid creating spaghetti monsters.
