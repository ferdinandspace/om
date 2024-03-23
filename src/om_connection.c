#include "om_connection.h"

#include "om_constants.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

void om_fatal(char* message) {
    char str[strlen(message) + 3];
    strcpy(str, message);
    strcat(str, "%s\n");
    printf(str, strerror(errno));
    exit(1);
}

int om_setup_nonblocking(int sockfd) {
    return fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK );
}

void om_epoll_ctl_add(int epoll_fd, int sock_fd, uint32_t events) {
    struct epoll_event e;
    e.events = events;
    e.data.fd = sock_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &e) == -1)
        om_fatal("epoll_ctl error");
}

int om_callback(const int id, const char* data, const int len, struct om_connction_parameters* p) {
    int sock_fd = id; //todo mapping between id and socket fd, now id is sock_fd
    send(sock_fd, data, len, 0);
    if(p != NULL && p->close_connection == 1)
        shutdown(sock_fd, SHUT_RDWR);
    return 0;
}

int om_listen_socket_init(const uint16_t port) {

    int fd;
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        om_fatal("socket: error");

    int yes = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        om_fatal("setsockopt: so_reuseaddr");

    int flag = 1;
    if(setsockopt(fd, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag)) == -1)
        om_fatal("setsockopt: tcp_nodelay");

    struct sockaddr_in addr; 
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    memset(&(addr.sin_zero), '\0', 8);
  
    if(bind(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1)
        om_fatal("bind: error");

    om_setup_nonblocking(fd);

    if(listen(fd, OM_MAX_CONNECTIONS) == -1)
        om_fatal("listen: error");

    return fd;
}

void om_connection_init(uint16_t port, int (*on_data_init)(int), int (*on_data_destroy)(int), int (*on_data)(int, char*, int, int(*)(int, const char*, int, struct om_connction_parameters*))) {

    char buffer[OM_IN_BUFFER_LENGTH + 1];

    struct epoll_event events[OM_MAX_CONNECTIONS];
    int epoll_fd = epoll_create(1);
    int listen_sock = om_listen_socket_init(port);

    om_epoll_ctl_add(epoll_fd, listen_sock, EPOLLIN | EPOLLOUT | EPOLLET);

    while(1) {
        int n = epoll_wait(epoll_fd, events, OM_MAX_CONNECTIONS, -1);
        
        for(int i = 0; i < n; i++) {
            
            if(events[i].events & (EPOLLRDHUP | EPOLLHUP)) { //connection has been closed
                
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                close(events[i].data.fd);
                on_data_destroy(events[i].data.fd);

            } else if(events[i].data.fd == listen_sock) { // new connection
                
                struct sockaddr_in client_addr;
                socklen_t socklen;
				int conn_sock = accept(listen_sock, (struct sockaddr *) &client_addr, &socklen);

				inet_ntop(AF_INET, (char *) &(client_addr.sin_addr), buffer, sizeof(client_addr));
				
                om_setup_nonblocking(conn_sock);
				om_epoll_ctl_add(epoll_fd, conn_sock, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP); 
                on_data_init(conn_sock); 

            } else if (events[i].events & EPOLLIN) { //process data
                
                int n = recv(events[i].data.fd, buffer, sizeof(buffer), 0);
                if(n > 0)       
                    on_data(events[i].data.fd, buffer, n, om_callback);

            } else { //unexpected
               //NO OP   
            }
        }
    }
}

