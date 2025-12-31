#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define MAX 80

void func(int connfd)
{
    char buff[MAX];
    int n;
    for(;;){
        bzero(buff, MAX);
    }
    read(connfd, buff, sizeof(buff));
    printf("From client %s\t to client : ", buff);
    bzero(buff, MAX);
    n = 0;
    while((buff[n++] = getchar()) != '\n');
    write(connfd, buff, sizeof(buff));
    
}
/* 
Create a TCP server that listens on port 8080
When an ESP32 connects, spawn a new thread to handle that connection
Each thread continuously reads data from its ESP32
Label the data so you know which device it came from
*/