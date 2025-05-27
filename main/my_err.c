#include "my_err.h"

static const char* strings[] = {

    [OK] = "OK",
    [CSV_CANNOT_OPEN_FILE] = "CSV_CANNOT_OPEN_FILE",
    [CSV_LINE_FORMAT] = "CSV_LINE_FORMAT",
    [CSV_WRONG_ORDER] = "CSV_WRONG_ORDER",
    [CSV_UNKNOWN_FIELD] = "CSV_UNKNOWN_FIELD",
    [PARSER_GETLINE] = "PARSER_GETLINE",
    [TOO_MANY_TRAINS] = "TOO_MANY_TRAINS",
    [CANNOT_CONVERT_TO_LED] = "CANNOT_CONVERT_TO_LED",
    [SNTP_SYNC_FAIL] = "SNTP_SYNC_FAIL",
    [WIFI_STA_CANNOT_CONNECT] = "WIFI_STA_CANNOT_CONNECT",

};

const char* strerr(my_err_t err){
    return strings[err];
}