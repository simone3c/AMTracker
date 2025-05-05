#ifndef MY_ERR
#define MY_ERR

typedef enum{
    NO_ERR = 0,
    PARSER_GETLINE,
    TOO_MANY_TRAINS,
    CSV_LINE_FORMAT,
    CSV_WRONG_ORDER,
    CSV_CANNOT_OPEN_FILE,
    CSV_UNKNOWN_FIELD,
    CANNOT_CONVERT_TO_LED,

    UNKNOW_ERR
} my_err_t;

const char* strerr(my_err_t err);

#endif