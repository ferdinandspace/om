#ifndef OM_CONNECTION_H
#define OM_CONNECTION_H

#include <stdint.h>


struct om_connction_parameters {
    
    short close_connection;
};

void om_connection_init(uint16_t port, int (*on_data_init)(int), int (*on_data_destroy)(int), int (*on_data)(int, char*, int, int(*)(int, const char*, int, struct om_connction_parameters*)));

#endif

