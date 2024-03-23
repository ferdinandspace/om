
#include "om_server.h"

#include "om_constants.h"
#include "om_connection.h"
#include "om_constants.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define NONE_ID -1

static const char* OM_EXPECTED_METHOD = "POST /om";

static const int OM_EXPECTED_METHOD_LENGTH = 8;

static const char* OM_RESPONSE_BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";

static const int OM_RESPONSE_BAD_REQUEST_LENGTH = 47;

static const char* OM_START_RESPONSE_OK = "HTTP/1.1 200 OK\r\nContent-Length: ";

static const int OM_START_RESPONSE_OK_LENGTH = 33; 

static const char* OM_RESPONSE_ERROR = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";

static const int OM_RESPONSE_ERROR_LENGTH = 57; 


typedef enum om_method_state {
    PENDING,
    WRONG,
    FOUND
} om_method_state;

struct request {

    int id;

    char data[OM_IN_BUFFER_LENGTH + 1];

    int position;

    int content_len;

    om_method_state method_state;

    int body_start;
};

static int(*process)(char* const, const int, char*, int*);

static struct request requests[OM_MAX_CONNECTIONS];

static struct om_connction_parameters close_connection_param = {.close_connection = 1};

int om_on_data_init(int id) {
    for (int i = 0; i < OM_MAX_CONNECTIONS; i++)
        if(requests[i].id == NONE_ID) {
            requests[i].id = id;
            requests[i].position = 0;
            requests[i].content_len = -1;
            requests[i].method_state = PENDING;
            requests[i].body_start = -1;
            break;  
        }
    return 0;
}

struct request* om_find_request(int id) {
    for(int i = 0; i < OM_MAX_CONNECTIONS; i++)
        if(requests[i].id == id)
            return &requests[i];
    return NULL;
}

int om_find_content_length(struct request* r) {
    r->data[r->position] = '\0';
    char* c = strstr(r->data + OM_EXPECTED_METHOD_LENGTH, "Content-Length: ");
    if(c == NULL)
        return -1;
    int n = atoi(c + 16); // 16 - length of "Content-Length " string 
    return n;
}

int om_find_body(struct request* r, int* start) {
    int body_start = r->body_start;
    if(body_start == -1)
        for(int i = OM_EXPECTED_METHOD_LENGTH; i < r->position - 3; i++)
            if(memcmp(&(r->data[i]), "\r\n\r\n", 4) == 0) { 
                body_start = i + 4;
                break;
            }
    *start = body_start;
    return body_start == -1 || r->position - r->body_start < r->content_len ? -1 : r->content_len; //-1 wait all body
}

int om_make_response(char* response_body, int response_body_len, char* response) {
    char content_len_str[5]; //5 - max len of content-length string
    sprintf(content_len_str, "%d", response_body_len);
    int n = strlen(content_len_str);
    memcpy(response, OM_START_RESPONSE_OK, OM_START_RESPONSE_OK_LENGTH);
    memcpy(response + OM_START_RESPONSE_OK_LENGTH, content_len_str, n); 
    memcpy(response + OM_START_RESPONSE_OK_LENGTH + n, "\r\n\r\n", 4);
    memcpy(response + OM_START_RESPONSE_OK_LENGTH + n + 4, response_body, response_body_len);
    return OM_START_RESPONSE_OK_LENGTH + n + 4 + response_body_len;
}

om_method_state om_check_method(struct request* r) {
    if(r->method_state == FOUND)
        return FOUND;
    if(r->position < OM_EXPECTED_METHOD_LENGTH)
        return PENDING;
    return memcmp(OM_EXPECTED_METHOD, r->data, OM_EXPECTED_METHOD_LENGTH) == 0 ? FOUND : WRONG;
}

void om_drop_packet(struct request* r) {
    int end = r->body_start + r->content_len;
    memmove(r->data, &(r->data[end]), r->position - end);
    r->content_len = -1;
    r->position = r->position - end;
    r->method_state = PENDING;
    r->body_start = -1;
}

int om_on_data(int id, char* data, int len, int(*callback)(int, const char*, int, struct om_connction_parameters*)) {

    struct request* r = om_find_request(id);

    if(r == NULL)
        return -1;

    if(r->position + len > OM_IN_BUFFER_LENGTH) {
        callback(id, OM_RESPONSE_BAD_REQUEST, OM_RESPONSE_BAD_REQUEST_LENGTH, &close_connection_param);
        return 0;
    }

    //copy bytes to request buffer
    memcpy(r->data + r->position, data, len);
    r->position += len;

    //check method POST /om
    if(r->method_state == PENDING && (r->method_state = om_check_method(r)) == PENDING) //wait all method line
        return 0;

    if(r->method_state == WRONG) {
        callback(id, OM_RESPONSE_BAD_REQUEST, OM_RESPONSE_BAD_REQUEST_LENGTH, &close_connection_param);
        return 0;
    }
    
    //find content length
    if(r->content_len == -1 && (r->content_len = om_find_content_length(r)) == -1) 
        return 0;

    //find body
    if(om_find_body(r, &(r->body_start)) != r->content_len)
        return 0;
   
    //process body 
    char response_body[OM_MAX_RESPONSE_BODY_LENGTH];
    int response_body_len;
    int res = process(r->data + r->body_start, r->content_len, response_body, &response_body_len);

    //drop precessed data
    om_drop_packet(r);

    //create end send response
    if (res == 0) {
        char response[OM_START_RESPONSE_OK_LENGTH + 5 + 4 + response_body_len]; //5 symbols for content length value + 2 symbols for '\n\n' before body
        int response_len = om_make_response(response_body, response_body_len, response);
        callback(id, response, response_len, NULL);
    } else {
        callback(id, OM_RESPONSE_ERROR, OM_RESPONSE_ERROR_LENGTH, &close_connection_param);
    }
    return 0;
}

int om_on_data_destroy(int id) {
    for(int i = 0; i < OM_MAX_CONNECTIONS; i++)
        if(requests[i].id == id) {
            requests[i].id = NONE_ID;
            break;
        }
    return 0;
}

int om_server(uint16_t port, int (*proc)(char* const, const int, char*, int*)) {
    process = proc;
    for(int i = 0; i < OM_MAX_CONNECTIONS; i++)
        requests[i].id = NONE_ID;
    om_connection_init(port, om_on_data_init, om_on_data_destroy, om_on_data);
    return 0;
}

int is_response_size_available(int size) {
    return size <= OM_MAX_RESPONSE_BODY_LENGTH;
}
