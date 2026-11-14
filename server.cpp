#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(){
    struct addrinfo * servinfo;
    struct addrinfo hints;
    struct sockaddr_storage their_addr;

    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;

    if((status = getaddrinfo(NULL, "8001", &hints, &servinfo)) != 0){
        fprintf(stderr, "get address info error: %s\n", gai_strerror(status));
        exit(1);
    }

    int s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    bind(s, servinfo->ai_addr, servinfo->ai_addrlen);

    listen(s, 25);
    socklen_t addr_size = sizeof their_addr;

    //file descriptor for connection
    int conn_fd = accept(s, (struct sockaddr *) &their_addr, &addr_size); 
}