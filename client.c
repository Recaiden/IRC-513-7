#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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

    // TODO use hostname command to resolve names to IP rather than hardcoding
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (cmd_connect(sock, server) > 0)
      return 1;
     
    // Read in line, compare to commands, default to sending as message.
    while(1)
    {
        printf("Send : ");
        scanf("%s", buffer_send);

	if(strncmp(buffer_send, "/HELP", 5) == 0)
	  cmd_help();
	else if(strncmp(buffer_send, "/FLAG", 5) == 0)
	  cmd_flag();
	else if(strncmp(buffer_send, "/QUIT", 5) == 0)
	  cmd_quit();
	else if(strncmp(buffer_send, "/FILE", 5) == 0)
	  cmd_transfer();
	else if(strncmp(buffer_send, "/JOIN", 5) == 0)
	  cmd_chat();
	else
	  cmd_send(sock);
         
        //Receive a reply from the server
        if( recv(sock, buffer_recv, 2000, 0) < 0)
        {
            puts("Recv failed");
            break;
        }
	else
	{
	  puts("Server reply :");
	  puts(buffer_recv);
	}
    }   
    close(sock);
    return 0;
}


int cmd_connect(int sock, struct sockaddr_in server)
{
  // Connect to TextRouletteServer
  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    perror("Error. connect failed.");
    return 1;
  }

  //TODO add recv for ACK
  puts("Connected\n");
  return 0;
}

int cmd_chat(char* id_channel)
{
}

int cmd_flag(char* id_channel)
{
}

int cmd_help()
{
}

int cmd_quit()
{
}

int cmd_transfer()
{
}

int cmd_send(int sock)
{
  //Send some data
  if(send(sock, buffer_send, strlen(buffer_send), 0) < 0)
  {
    puts("Send failed");
    return 1;
  }
}
