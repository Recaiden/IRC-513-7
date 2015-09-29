#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include "global.h"

char buffer_send[1000];
char buffer_recv[2000];


 
int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in server;
     
    //Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
 
    //Connect to remote server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }
     
    puts("Connected\n");
     
    //keep communicating with server
    while(1)
    {
        printf("Enter buffer_send : ");
        scanf("%s", buffer_send);
         
        //Send some data
        if( send(sock, buffer_send, strlen(buffer_send), 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
         
        //Receive a reply from the server
        if( recv(sock, buffer_recv, 2000, 0) < 0)
        {
            puts("Recv failed");
            break;
        }
         
        puts("Server reply :");
        puts(buffer_recv);
    }
     
    close(sock);
    return 0;
}
