#include "my_string.h"
#include <stdlib.h>
#include <string.h>

static void buffer_resize(my_string* s, size_t new_cap){
    s->data = realloc(s->data, new_cap);
    assert(s->data != NULL);
    s->cap = new_cap;
}

void my_string_init(my_string* s){
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

void my_string_delete(my_string* s){
    free(s->data);
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

size_t my_string_init_from_array(my_string* s, const char* from, size_t from_len){
    my_string_init(s);
    buffer_resize(s, from_len + from_len / 2);
    s->len = from_len;
    memcpy(s->data, from, s->len);
    s->data[s->len] = 0;
    return s->len;
}

size_t my_string_init_from_my_string(my_string* s, const my_string* from){
    my_string_init(s);
    buffer_resize(s, from->cap);
    s->len = from->len;
    memcpy(s->data, from->data, from->len);
    s->data[s->len] = 0;
    return s->len;
}

int my_string_strstr(const my_string* s, const char* substring){
    char* ptr = strstr(s->data, substring);
    if(ptr != NULL){
        return ptr - s->data; 
    }

    return -1;
}

size_t my_string_insert_array_at(my_string* s, int at, const char* substring, size_t sub_len){

    if(substring == NULL){
        return s->len;
    }
  
    size_t new_len = s->len + sub_len;
    if(new_len >= s->cap){
        buffer_resize(s, new_len + new_len / 2);
    }

    for(int i = new_len - 1; i >= at; --i){
        s->data[i] = s->data[i - sub_len];
    }
    for(int i = 0; i < sub_len; ++i){
        s->data[at + i] = substring[i];
    }
    s->data[new_len] = 0;
    s->len = new_len;
    return s->len;
}

size_t my_string_insert_at(my_string* s, int at, const my_string* substring){
    return my_string_insert_array_at(s, at, substring->data, substring->len);
}


