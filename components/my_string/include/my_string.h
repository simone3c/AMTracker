#ifndef MY_STRING
#define MY_STRING

#include <stddef.h>

typedef struct my_string{

    char* data;
    size_t len; // doesnt include string terminator
    size_t cap;

} my_string;

void my_string_init(my_string* s);
void my_string_delete(my_string* s);

size_t my_string_init_from_array(my_string* s, const char* from, size_t from_len);
size_t my_string_init_from_my_string(my_string* s, const my_string* from);

int my_string_strstr(const my_string* s, const char* substring);

size_t my_string_insert_array_at(my_string* s, int at, const char* substring, size_t sub_len);
size_t my_string_insert_at(my_string* s, int at, const my_string* substring);

#endif