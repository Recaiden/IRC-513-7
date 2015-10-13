#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/utsname.h>

#define PORT 6660

#define MAX_CLIENTS 10
#define MAX_SOCKETS  MAX_CLIENTS+5
#define PACKET_SIZE 1024

int flags [MAX_SOCKETS];
int blocks [MAX_SOCKETS];
char nicknames [MAX_SOCKETS][PACKET_SIZE];
int used_fds[MAX_SOCKETS];
int partners[MAX_SOCKETS];
int data_use[MAX_SOCKETS];

int running = 0;
int init = 0;


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
  char str2[PACKET_SIZE];
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
int sendToClient(char *message, int id)
{
  /*  char fd_id [5];
  char stripped_message [PACKET_SIZE-5];
  memcpy(fd_id, &message[0], 4);
  memcpy(stripped_message, &message[4], PACKET_SIZE-5);
  fd_id[4] = '\0';
  int id = strtoul(fd_id, NULL, 10);*/

  if(strstr(message, "/CONN") != NULL ||
     strstr(message, "/HELP") != NULL ||
     strstr(message, "/QUIT") != NULL ||
     strstr(message, "/CHAT") != NULL ||
     strstr(message, "/FLAG") != NULL)
    {
	return 0;
    }
  if(partners[id] > 1)
  {
    int n = write(partners[id], message, strlen(message));
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
  for(x = 0; x < MAX_SOCKETS; x++)
  {
    if(blocks[fd] != 0)
      break;
    if(blocks[x] != 0)
      continue;
    if(x != fd && used_fds[x] == 2)
    {
      matched = 1;
      int n;
      char str[PACKET_SIZE-5];
      char str2[PACKET_SIZE-5];

      // Write to partner who triggered connection
      sprintf(str, "%04d|%s", x, nicknames[x]);
      n = write(fd, str, strlen(str));
      if(n < 0)
      { perror("Error writing to client");  exit(1); }
      used_fds[fd] = 3;
      partners[fd] = x;

      // Write to waiting partner who was first to connect
      sprintf(str2, "%04d|%s", fd, nicknames[fd]);
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
    /*char fd_id [5];
    char stripped_message [PACKET_SIZE-5];
    memcpy(fd_id, &message[0], 4);
    memcpy(stripped_message, &message[4], PACKET_SIZE-5);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);*/
    int id = partners[fd];

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
    //char stripped_message [PACKET_SIZE-10];
    //memcpy(stripped_message, &message[5], PACKET_SIZE-10);
    //char fd_id [5];
    //memcpy(fd_id, &message[0], 4);
    //fd_id[4] = '\0';
    //int id = strtoul(fd_id, NULL, 10);

    memcpy(nicknames[fd], &message[5], strlen(message)-4);
    
    char response[PACKET_SIZE];
    bzero(response, PACKET_SIZE);
    sprintf(response, "You are now connected as %s", nicknames[fd]);
    int n = write(fd, response, strlen(response));
    if(n < 0)
    { perror("Error ACKing nickname");  return 1; }
  }
  else if(strstr(message, "/HELP") != NULL)
  {
    char response[PACKET_SIZE];
    bzero(response, PACKET_SIZE);
    sprintf(response, "/HELP The server accepts the following commands:\n/CHAT to enter a chatroom\n/FLAG to report misbehavior by your partner\n/FILE path/to/file to begin transferring a file to your partner\n/QUIT to leave your chat.");
    int n = write(fd, response, strlen(response));
    if(n < 0)
    { perror("Error writing Help message to client");  return 1; }
  }
  else if(strstr(message, "/QUIT") != NULL)
  {
    int n;
    /*char fd_id [5];
    memcpy(fd_id, &message[0], 4);
    fd_id[4] = '\0';
    int id = strtoul(fd_id, NULL, 10);*/
    int id = partners[fd];

    char str[PACKET_SIZE];
    char str2[PACKET_SIZE];

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
    /*char fd_id [5];
      char stripped_message [PACKET_SIZE-5];
      bzero(stripped_message, PACKET_SIZE-5);
      memcpy(fd_id, &message[0], 4);
      memcpy(stripped_message, &message[4], PACKET_SIZE-5);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);*/
    int id = partners[fd];
    int n = write(id,message, strlen(message));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }

  }
  else if(strstr(message, "/ENDF") != NULL){
    /*char fd_id [5];
      char stripped_message [PACKET_SIZE-5];
      bzero(stripped_message, PACKET_SIZE-5);
      memcpy(fd_id, &message[0], 4);
      memcpy(stripped_message, &message[4], PACKET_SIZE-5);
      fd_id[4] = '\0';
      int id = strtoul(fd_id, NULL, 10);*/
    int id = partners[fd];
    int n = write(id, message, strlen(message));
    if(n < 0)
    { perror("Error writing to client");  exit(1); }
  }
  else
  {
    char response[PACKET_SIZE];
    bzero(response, PACKET_SIZE);
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
    char buffer[PACKET_SIZE];
    bzero(buffer, PACKET_SIZE);
    fgets(buffer, PACKET_SIZE-1, stdin);

    if(strstr(buffer, "/STATS") != NULL)
    {
      FILE *fp;
      fp = fopen("stats.txt", "w+b");
      int x;
      int sQueue = 0, sChat =0;
      for(x = 0; x < MAX_SOCKETS; x++)
      {
	if(used_fds[x] == 2)
	  sQueue += 1;
	else if(used_fds[x] == 3)
	  sChat += 1;
	if(flags[x] != 0)
	{
	  printf("User %d - %s has been flagged %d times.\n", x, nicknames[x], flags[x]);
	  fprintf(fp, "User %d - %s has been flagged %d times.\n", x, nicknames[x], flags[x]);
	}
	if(blocks[x] != 0)
	{
	  printf("User %d - %s has been BLOCKED from entering chat.\n", x, nicknames[x]);
	  fprintf(fp, "User %d - %s has been BLOCKED from entering chat.\n", x, nicknames[x]);
	}
  if(partners[x]>0 && partners[x] > partners[partners[x]])
  {
    printf("ChatRoom with %s and %s data usage: %dbytes\n",nicknames[x], nicknames[partners[x]], (data_use[x]+data_use[partners[x]]) );
    fprintf(fp, "ChatRoom with %s and %s data usage: %dbytes\n",nicknames[x], nicknames[partners[x]], (data_use[x]+data_use[partners[x]]) );
  }
      }
      printf("%d users are in chat, and %d users are waiting in queue.\n", sChat, sQueue);
      fprintf(fp, "%d users are in chat, and %d users are waiting in queue.\n", sChat, sQueue);
      fclose(fp);
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
    char str[PACKET_SIZE];
    char str2[PACKET_SIZE];

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
      if(running == 0)
	beginServer();
      else
	printf("Server has already been started.\n");
    }
    else if(strstr(buffer, "/END") != NULL)
    {
      int id = 0;
      char str[PACKET_SIZE];
      for(id = 0; id < MAX_SOCKETS; id++) {
        if(used_fds[id] == 2){
          bzero(str, PACKET_SIZE);
          sprintf(str, "You've been removed from the queue by the server.\n");

          int n = write(id, str, strlen(str));
          if(n < 0)
          { perror("Error writing quit to queue");  return 1; }
          used_fds[id] = 1;

        }
        else if(used_fds[id] == 3){
          bzero(str, PACKET_SIZE);
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
      printf("Closing server socket.\n");
      shutdown((int)socket_fd, SHUT_RDWR);
      close((int)socket_fd);
      running = 0;
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
  char buffer[PACKET_SIZE];
  struct sockaddr_in server_addr, client_addr;
  int n, i, number_sockets = 0;
  int client_fd[MAX_SOCKETS];


  //init fd to socket type
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_fd < 0){
    perror("Error trying to open socket");
    exit(1);
  }

  // Only run once.
  if(init == 0)
  {
    init = 1;
  
    //init socket
    bzero((char *) &server_addr, sizeof(server_addr));  //zero out addr
    port_num = PORT; // set port number
  
    //server_addr will contain address of server
    server_addr.sin_family = AF_INET; // Needs to be AF_NET
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // contains ip address of host 
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
    int n = listen(socket_fd, MAX_SOCKETS);
    //printf("Listen result %d\n", n);
    client_len = sizeof(client_addr); // set client length
  }
  
  FD_ZERO(&fd_master_set);
  FD_SET(socket_fd, &fd_master_set);

  running = 1;

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
    if(number_sockets < MAX_CLIENTS)//MAX_SOCKETS)
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
    {
      printf("No more connection space\n");
      
      client_fd[number_sockets] = 
        accept(socket_fd, (struct sockaddr *)&client_addr, &client_len);
      if(client_fd[number_sockets] < 0)
      { perror("Error accepting client"); exit(1); }
      
      printf("Overflow client temporarily accepted.\n");
      FD_SET(client_fd[number_sockets], &fd_master_set);

      char* str = "/ERRThe queue is full, you cannot connect at this time.\n";
      int n = write(client_fd[number_sockets], str, strlen(str));
      if(n < 0)
      {
	perror("Error informing client of full queue");
	exit(1);
      }
      //used_fds[client_fd[number_sockets]] = 1;
      //FD_SET(client_fd[number_sockets], &fd_master_set);
      FD_CLR(client_fd[number_sockets], &fd_master_set);
      //number_sockets --;
    }
  } else {  
    //begin communication
    bzero(buffer, PACKET_SIZE);
    n = read(i, buffer, PACKET_SIZE-1);
    if(n < 0){
      perror("Error reading from client");
      // Do not exit, mark that client as D/C, inform their partner, carry one
      FD_CLR(i, &fd_master_set);
      FD_CLR(i, &read_set);
      handle_disconnect(i);
      number_sockets --;
      continue;
    }

    //debug, message coming in
    //printf(buffer);
    //printf("prebuffed: %s END\n", buffer);
    data_use[i] += strlen(buffer);
    //handles all messages
    if(sendToClient(buffer, i) == 0)
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
  char buffer[PACKET_SIZE];
  while(1){
    bzero(buffer, PACKET_SIZE);
    fgets(buffer, PACKET_SIZE-1, stdin);
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
  struct utsname unameData;
  int i = uname(&unameData);
  if(i != 0)
    perror("Could not find the server's own hostname.");
  else
  {
    printf("Machine Name: %s\n", unameData.sysname);
    printf("Host Name: %s\n", unameData.nodename);
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Hostname: %s\n", hostname);
    struct hostent* h;
    h = gethostbyname(hostname);
    printf("h_name: %s\n", h->h_name);
  }
  initScreen();
  return 0;
}
