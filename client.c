#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

int friend_fd = 0;

void *read_chat(void *socket)
{
  int n;
  char chat_buffer[256];
  int * socket_fd = (int *)socket;
  while(1){
    //read server response
    bzero(chat_buffer, 256);
    n = read((* socket_fd), chat_buffer, 255);
    if(n < 0){
      sleep(1); //sleep some time while waiting for a message
    } else {
      //display message
      //if(strlen(chat_buffer) < 2){
      if(friend_fd == 0 && strchr(chat_buffer, '|') != NULL){
	int index = strchr(chat_buffer, '|') - chat_buffer + 1;

	char fd_id [5];
	char stripped_message [251];
	memcpy(fd_id, &chat_buffer[0], index);
	memcpy(stripped_message, &chat_buffer[index], 256 - index -1);

	friend_fd = atoi(fd_id);
	
	printf("You are chatting with %s\n", stripped_message);
      } else {
	printf("%s\n", chat_buffer);
      }
    }
  }		
}

int main(int argc, char *argv[]){
  
  int socket_fd, port_num, n;
  struct sockaddr_in server_addr;
  struct hostent * server;
  char packaged[256];
  char buffer[251];
  
  //not given hostname and port
  if(argc <3){
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    exit(0);
  }
  
  port_num = atoi(argv[2]); //convert arg to int
  
  //init socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_fd < 0){
    perror("Error opening socket");
    exit(1);
  }
  
  server = gethostbyname(argv[1]);
  
  if(server == NULL){
    fprintf(stderr, "Error, no such host\n");
    exit(0);
  }
  
  bzero((char *) &server_addr, sizeof(server_addr)); //zero out addr
  server_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length); //copy over s_addr
  server_addr.sin_port = htons(port_num); //htons converts to network byte order
  
  //connect to server
  if(connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  { perror("Error connecting to server"); exit(1); }

  // Identifying Connect command
  printf("Please enter your nickname: ");
  //create message for server
  bzero(packaged, 255);
  bzero(buffer, 251);
  fgets(buffer, 250, stdin);
  sprintf(packaged, "%04d/CONN", friend_fd);
  strcat(packaged, buffer);
  
  //send message to server
  n = write(socket_fd, packaged, strlen(packaged));
  if(n < 0)
  { perror("Error identifying to server"); exit(1); }
  
  pthread_t reader_thread;
  
  if(pthread_create(&reader_thread, NULL, read_chat, &socket_fd)){
    fprintf(stderr, "Error creating reader thread\n");
    exit(1);
  }
  
  while(1){
    printf("Please enter a message: ");
    
    //create message for server
    bzero(packaged, 255);
    bzero(buffer, 251);
    fgets(buffer, 250, stdin);
    sprintf(packaged, "%04d", friend_fd);
    strcat(packaged, buffer);
    
    //send message to server
    n = write(socket_fd, packaged, strlen(packaged));
    if(n < 0)
    { perror("Error writing to server"); exit(1); }
  }	
  close(socket_fd);
  
}

