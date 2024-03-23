#ifndef OM_SERVER_H
#define OM_SERVER_H

#include <stdint.h>

int om_server(uint16_t port, int(*proc)(char* const, const int, char*, int*));

int is_response_size_available(int size);

#endif

