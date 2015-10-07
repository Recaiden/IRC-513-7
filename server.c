#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define PORT 6660

#define MAX_SOCKETS  10

int flags [MAX_SOCKETS];


/*
	sendToClient uses the fd appended to the beginning of 
	messages to find who the message should be sent to
*/
int sendToClient(char *message)
{
  char fd_id [5];
  char stripped_message [251];
  memcpy(fd_id, &message[0], 4);
  memcpy(stripped_message, &message[4], 251);
  fd_id[4] = '\0';
  int id = strtoul(fd_id, NULL, 10);
  //printf("id: %d\n", id);
  if(id > 1)
  {
    int n = write(id, stripped_message, strlen(stripped_message));
    if(n < 0)
    {
      perror("Error writing to client");
      exit(1);
    }
    return 1; 
  }
  return 0;
} 

// used_fds
// 1 is just connected
// 2 is waiting to find partner
// 3 is connected

int processConnect(char *message, int fd, int* used_fds)
{
  int x;
  int matched = 0;
  for(x = 0; x < MAX_SOCKETS+5; x++)
  {
    if(x != fd && used_fds[x] == 2)
    {
      matched = 1;
      int n;
      char str[5];
      char str2[5];
      
      sprintf(str, "%d", x);
      n = write(fd, str, strlen(str));
      if(n < 0)
      { perror("Error writing to client");  exit(1); }
      
      sprintf(str2, "%d", fd);
      n = write(x, str2, strlen(str2));
      if(n < 0)
      { perror("Error writing to client");  exit(1); }
	
      used_fds[x] = 3;
      used_fds[fd] = 3;
      printf("Matched! Connecting %d and %d\n", x, fd);
      break;
    } 
  }
  if(matched == 0)
  {
    printf("no match\n");
    used_fds[fd] = 2; // 2 is fds waiting to find partners
  }
  return 0;
}



// fd is sending Client's number
int processCommand(char *message, int fd, int* used_fds)
{
  printf("Processing...%d\n", fd);
  if(strstr(message, "/CHAT") != NULL)
  { processConnect(message, fd, used_fds); }
  else if(strstr(message, "/FLAG") != NULL)
  {
    // Flag fd's partner
    char fd_id [5];
    char stripped_message [251];
    memcpy(fd_id, &message[0], 4);
    memcpy(stripped_message, &message[4], 251);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);
    
    flags[id] = 1; // Currently has no effect
    //TODO error-handling - FLAG before CHAT should do nothing
  }
  else if(strstr(message, "/HELP") != NULL)
  {
    char response[256];
    bzero(response, 256);
    sprintf(response, "The server accepts the following commands:\n/CHAT to enter a chatroom\n/FLAG to report misbehavior by your partner\n/FILE to begin transferring a file to your partner\n/QUIT to leave your chat.");
    int n = write(fd, response, strlen(response));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }
  }
  else if(strstr(message, "/QUIT") != NULL)
  {
    int n;
    char fd_id [5];
    memcpy(fd_id, &message[0], 4);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);

    char str[256];
    char str2[256];

    // Disconnect requester
    sprintf(str, "You have disconnected from the chatroom.  You will return to the queue.\n");
    n = write(fd, str, strlen(str));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }
    used_fds[fd] = 1;

    // Disconnect their partner
    sprintf(str2, "Your partner has disconnected from the chatroom.  You will return to the queue.\n");
    n = write(id, str2, strlen(str2));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }
    used_fds[id] = 1;
  }
  else if(strstr(message, "/FILE") != NULL)
  {
    //
    perror("unfinished code.");
  }
  else
  {
    char response[256];
    bzero(response, 256);
    sprintf(response, "Command not Found");
    int n = write(fd, response, strlen(response));
    if(n < 0)
      { perror("Error writing to client");  exit(1); }
  }
  // compare to keywords for connecting chats blocking people etc.
  return 0;
}


int main (int argc, char *argv[] ){

  int socket_fd, port_num, client_len;
  fd_set fd_master_set, read_set;
  char buffer[256];
  struct sockaddr_in server_addr, client_addr;
  int n, i, number_sockets = 0;
  int client_fd[MAX_SOCKETS];
  int used_fds[MAX_SOCKETS+5];
  
  //init fd to socket type
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_fd < 0){
    perror("Error trying to open socket");
    exit(1);
  }
  
  //init socket
  bzero((char *) &server_addr, sizeof(server_addr));  //zero out addr
  port_num = PORT; // set port number
  
  //server_addr will contain address of server
  server_addr.sin_family = AF_INET; // Needs to be AF_NET
  server_addr.sin_addr.s_addr = INADDR_ANY; // contains ip address of host 
  server_addr.sin_port = htons(port_num); //htons converts to network byte order
  
  int yes = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
    perror("setsockopt"); 
    exit(1); 
  }  
  
  //bind the host address with bind()
  if(bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
    perror("Error Binding");
    exit(1);
  }
  printf("Server listening for sockets on port:%d\n", port_num);
  
  //listen for clients
  listen(socket_fd, 10);
  client_len = sizeof(client_addr); // set client length
  
  FD_ZERO(&fd_master_set);
  FD_SET(socket_fd, &fd_master_set);
  
  
  while(1){
    
    read_set = fd_master_set;
    n = select(FD_SETSIZE, &read_set, NULL, NULL, NULL);
    if(n < 0){
      perror("Select Failed");
      exit(1);
    }
    for(i = 0; i < FD_SETSIZE; i++){
      if(FD_ISSET(i, &read_set)){
	if(i == socket_fd){
	  if(number_sockets < MAX_SOCKETS){
	    //accept new connection from client
	    client_fd[number_sockets] = 
	      accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);
	    if(client_fd[number_sockets] < 0){
	      perror("Error accepting client");
	      exit(1);
	    }
	    printf("Client accepted.\n");
	    used_fds[client_fd[number_sockets]] = 1;
	    FD_SET(client_fd[number_sockets], &fd_master_set);
	    number_sockets++;

	    // Create chatroom
	  } else {
	    printf("No more connection space");
	  }
	} else {  
	  //begin communication
	  bzero(buffer, 256);
	  n = read(i, buffer, 255);
	  if(n < 0){
	    perror("Error reading from client");
	    exit(1);
	  }

	  //debug, message coming in
	  printf(buffer);
	  
	  //handles all messages
	  if(sendToClient(buffer) == 0)
	  {
	    processCommand(buffer, i, used_fds);
	  }
	  
	}
      }
    }
  }
  return 0;
}
