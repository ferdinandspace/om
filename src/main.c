

#include "om_server.h"
#include <stdio.h>
#include <string.h>


int proc(char* const request, const int request_len, char* response, int* respose_len);

int main(int argc, char* argv[]) {

    om_server(5000, proc);
    return 0;    
}

int proc(char* const request, const int request_len, char* response, int* respose_len) {

    if(is_response_size_available(request_len)) {
        memcpy(response, request, request_len);
        *respose_len = request_len;
  
        char response_str[request_len + 1];
        memcpy(response_str, response, request_len);
        response_str[request_len] = '\0';
        printf("main: body for processing: %s\n", response_str);
    } else {
        printf("server needs more response size\n");
        return 1;
    }
    
    return 0;
}

