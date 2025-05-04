#ifndef MY_WIFI
#define MY_WIFI

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "nvs_flash.h"

#define WIFI_SUCCESS 1
#define WIFI_FAILURE 2
#define MAX_FAILURES 10

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct addr_in addr_in;

void setup_wifi();
void wifi_stop();

#endif