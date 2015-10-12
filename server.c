#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 6660

#define MAX_SOCKETS  10

int flags [MAX_SOCKETS];
int blocks [MAX_SOCKETS];
char nicknames [MAX_SOCKETS][256];
int used_fds[MAX_SOCKETS+5];
int partners[MAX_SOCKETS];
int data_use[MAX_SOCKETS];


int reset_partners(int id_quitting)
{
  partners[partners[id_quitting]] = 0;
  partners[id_quitting] = 0;
  data_use[partners[id_quitting]] = 0;
  data_use[id_quitting] = 0;

  return 0;
}

int handle_disconnect(int i)
{
  char str2[256];
  int n;
  
  // Disconnect their partner
  sprintf(str2, "/PARTYour partner has disconnected from the server.  You will return to the queue.\n");
  n = write(partners[i], str2, strlen(str2));
  if(n < 0)
  { perror("Error writing to client");  return(1); }
  used_fds[partners[i]] = 1;
  
  used_fds[i] = 0;
  blocks[i] = 0;
  flags[i] = 0;
  reset_partners(i);
  
  return 0;
}

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

  if(strchr(stripped_message, '/') != NULL)
    return 0;
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

int processConnect(char *message, int fd)//, int* used_fds)
{
  int x;
  int matched = 0;
  for(x = 0; x < MAX_SOCKETS+5; x++)
  {
    if(blocks[fd] != 0)
      break;
    if(blocks[x] != 0)
      continue;
    if(x != fd && used_fds[x] == 2)
    {
      matched = 1;
      int n;
      char str[251];
      char str2[251];

      // Write to partner who triggered connection
      sprintf(str, "%d|%s", x, nicknames[x]);
      n = write(fd, str, strlen(str));
      if(n < 0)
      { perror("Error writing to client");  exit(1); }
      used_fds[fd] = 3;
      partners[fd] = x;

      // Write to waiting partner who was first to connect
      sprintf(str2, "%d|%s", fd, nicknames[fd]);
      n = write(x, str2, strlen(str2));
      if(n < 0)
      { perror("Error writing to client");  exit(1); }
      used_fds[x] = 3;
      partners[x] = fd;
      
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
int processCommand(char *message, int fd)//, int* used_fds)
{
  printf("Processing command message from User %d\n", fd);
  if(strstr(message, "/CHAT") != NULL)
    { processConnect(message, fd); }//, used_fds); }
  else if(strstr(message, "/FLAG") != NULL)
  {
    // Flag fd's partner
    char fd_id [5];
    char stripped_message [251];
    memcpy(fd_id, &message[0], 4);
    memcpy(stripped_message, &message[4], 251);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);

    printf("User %d flagged by User %d\n", id, fd);

    // check if the client being flagged is actually still conencted
    if(used_fds[id] == 3)
    {
      printf("Flagging done\n");
      flags[id] += 1; // Currently has no effect
    }
  }
  else if(strstr(message, "/CONN") != NULL)
  {
    char stripped_message [246];
    memcpy(stripped_message, &message[9], 246);
    //char fd_id [5];
    //memcpy(fd_id, &message[0], 4);
    //fd_id[4] = '\0';
    //int id = strtoul(fd_id, NULL, 10);

    memcpy(nicknames[fd], stripped_message, 246);
    
    char response[356];
    bzero(response, 356);
    sprintf(response, "You are now connected as %s", nicknames[fd]);
    int n = write(fd, response, strlen(response));
    if(n < 0)
    { perror("Error ACKing nickname");  return 1; }
  }
  else if(strstr(message, "/HELP") != NULL)
  {
    char response[256];
    bzero(response, 256);
    sprintf(response, "/HELP The server accepts the following commands:\n/CHAT to enter a chatroom\n/FLAG to report misbehavior by your partner\n/FILE path/to/file to begin transferring a file to your partner\n/QUIT to leave your chat.");
    int n = write(fd, response, strlen(response));
    if(n < 0)
    { perror("Error writing Help message to client");  return 1; }
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
    sprintf(str, "/PARTYou have left from the chatroom.  You will return to the queue.\n");
    n = write(fd, str, strlen(str));
    if(n < 0)
    { perror("Error writing quit to quitter");  return 1; }
    used_fds[fd] = 1;

    // Disconnect their partner
    sprintf(str2, "/PARTYour partner has left from the chatroom.  You will return to the queue.\n");
    n = write(id, str2, strlen(str2));
    if(n < 0)
    { perror("Error writing quit to partner");  return 1; }
    used_fds[id] = 1;

    reset_partners(fd);
  }
  else if(strstr(message, "/FILE") != NULL)
  {
      char fd_id [5];
      char stripped_message [251];
      bzero(stripped_message, 251);
      memcpy(fd_id, &message[0], 4);
      memcpy(stripped_message, &message[4], 251);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);
    int n = write(id, stripped_message, strlen(stripped_message));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }

  }
  else if(strstr(message, "/ENDF") != NULL){
      char fd_id [5];
      char stripped_message [251];
      bzero(stripped_message, 251);
      memcpy(fd_id, &message[0], 4);
      memcpy(stripped_message, &message[4], 251);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);
    int n = write(id, stripped_message, strlen(stripped_message));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }
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

int admin_commands(void *socket)
{
  int * socket_fd = (int *)socket;
  while(1)
  {
    // read user commands and parse them
    char buffer[256];
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);

    if(strstr(buffer, "/STATS") != NULL)
    {
      int x;
      int sQueue = 0, sChat =0;
      for(x = 0; x < MAX_SOCKETS; x++)
      {
	if(used_fds[x] == 2)
	  sQueue += 1;
	else if(used_fds[x] == 3)
	  sChat += 1;
	if(flags[x] != 0)
	  printf("User %d - %s has been flagged %d times.\n", x, nicknames[x], flags[x]);
	if(blocks[x] != 0)
	  printf("User %d - %s has been BLOCKED from entering chat.\n", x, nicknames[x]);
  if(partners[x]>0 && partners[x] > partners[partners[x]]){
    printf("ChatRoom with %s and %s data usage: %dbytes\n",nicknames[x], nicknames[partners[x]], (data_use[x]+data_use[partners[x]]) );
  }
      }
      printf("%d users are in chat, and %d users are waiting in queue.\n", sChat, sQueue);
    }
    // takes a user's numerical ID
    else if(strstr(buffer, "/BLOCK") != NULL)
    {
      char fd_id [5];
      memcpy(fd_id, &buffer[0], 4);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);
      if(id == 0 || id > MAX_SOCKETS)
	perror("Invalid cient ID\n");
      else
	blocks[id] = 1;
    }
    // takes a user's numerical ID
    else if(strstr(buffer, "/UNBLOCK") != NULL)
    {
      char fd_id [5];
      memcpy(fd_id, &buffer[0], 4);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);
      if(id == 0 || id > MAX_SOCKETS)
	perror("Invalid cient ID\n");
      else
	blocks[id] = 0;
    }
    else if(strstr(buffer, "/THROW") != NULL)
      {
	  int n;
    char fd_id [5];
    memcpy(fd_id, &buffer[7], 4);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);
    int fd = partners[id];
    char str[256];
    char str2[256];

    // Disconnect requester
    sprintf(str, "/PARTYour Channel has been ended by the admin.\n");
    n = write(fd, str, strlen(str));
    if(n < 0)
    { perror("Error writing quit to quitter");  return 1; }
    used_fds[fd] = 1;

    // Disconnect their partner
    sprintf(str2, "/PARTYour Channel has been ended by the admin.\n");
    n = write(id, str2, strlen(str2));
    if(n < 0)
    { perror("Error writing quit to partner");  return 1; }
    used_fds[id] = 1;

    reset_partners(id);
    
    }
    else if(strstr(buffer, "/START") != NULL)
      {
        printf("Server has already been started.\n");
    }
    else if(strstr(buffer, "/END") != NULL)
    {
      int id = 0;
      char str[256];
      for(id = 0; id < MAX_SOCKETS; id++) {
        if(used_fds[id] == 2){
          bzero(str, 256);
          sprintf(str, "You've been removed from the que by the server.\n");

          int n = write(id, str, strlen(str));
          if(n < 0)
          { perror("Error writing quit to partner");  return 1; }
          used_fds[id] = 1;

        }
        else if(used_fds[id] == 3){
          bzero(str, 256);
          sprintf(str, "/PARTYour Channel has been ended by the admin.\n");

          int n = write(id, str, strlen(str));
          if(n < 0)
          { perror("Error writing quit to partner");  return 1; }
          used_fds[id] = 1;

          n = write(partners[id], str, strlen(str));
          if(n < 0)
          { perror("Error writing quit to partner");  return 1; }
          used_fds[partners[id]] = 1;
          
          reset_partners(id);
        }
      }

    }
    else
    {
      printf("Unknown admin command.\n");
    }
  }
}

int beginServer(){
  int socket_fd, port_num, client_len;
  fd_set fd_master_set, read_set;
  char buffer[256];
  struct sockaddr_in server_addr, client_addr;
  int n, i, number_sockets = 0;
  int client_fd[MAX_SOCKETS];

  
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
  listen(socket_fd, MAX_SOCKETS);
  client_len = sizeof(client_addr); // set client length
  
  FD_ZERO(&fd_master_set);
  FD_SET(socket_fd, &fd_master_set);

  pthread_t admin_thread;
  
  if(pthread_create(&admin_thread, NULL, (void *)admin_commands, &socket_fd))
  { fprintf(stderr, "Error creating admin thread\n"); exit(1);}
  
  while(1){
    
    read_set = fd_master_set;
    n = select(FD_SETSIZE, &read_set, NULL, NULL, NULL);
    if(n < 0)
    { perror("Select Failed"); exit(1); }
    
    for(i = 0; i < FD_SETSIZE; i++)
    {
      if(FD_ISSET(i, &read_set))
      {
  if(i == socket_fd)
  {
    if(number_sockets < MAX_SOCKETS)
    {
      //accept new connection from client
      client_fd[number_sockets] = 
        accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);
      if(client_fd[number_sockets] < 0)
      { perror("Error accepting client"); exit(1); }
      printf("Client accepted.\n");
      used_fds[client_fd[number_sockets]] = 1;
      FD_SET(client_fd[number_sockets], &fd_master_set);
      number_sockets++;
    }
    else
    { printf("No more connection space"); }
  } else {  
    //begin communication
    bzero(buffer, 256);
    n = read(i, buffer, 255);
    if(n < 0){
      perror("Error reading from client");
      // Do not exit, mark that client as D/C, inform their partner, carry one
      FD_CLR(i, &fd_master_set);
      FD_CLR(i, &read_set);
      handle_disconnect(i);
      continue;
    }

    //debug, message coming in
    //printf(buffer);
    //printf("prebuffed: %s END\n", buffer);
    data_use[i] += strlen(buffer);
    //handles all messages
    if(sendToClient(buffer) == 0)
    {
      processCommand(buffer, i);//, used_fds);
    }
    
  }
      }
    }
  }
  return 0;
}

int initScreen(){
  printf("To being accepting clients type /START\n");
  char buffer[256];
  while(1){
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    if(strstr(buffer, "/START") != NULL){
      break;
    }
  }
  beginServer();
  return 0;
}

int main (int argc, char *argv[] ){
  memset(data_use, 0, sizeof(data_use));
  printf("Welcome to the CS513 Chat Server.\n");
  initScreen();
  return 0;
}
