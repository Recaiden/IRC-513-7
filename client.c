#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#define SIZE_FILE_MAX 104857600
#define PACKET_SIZE 1024

/*
Because our chat system does not follow a send/response protocol 
We need to have a millisecond delay in order to avoid multiple messages from 
being sent into a single buffer to the server.  
*/
struct timespec ts;

int friend_fd = 0;
char friend_nick [PACKET_SIZE];
int receivingFile = 0;
FILE *file;

int transfer_file(int socket_fd, char* filename)
{
  char packaged[PACKET_SIZE];
  char buffer[PACKET_SIZE-5];
  
  FILE *fp;
  fp = fopen(filename, "rb"); 
  if(fp == NULL)
  { perror("Error opening file"); return 1; }

  while(1)
  {
    bzero(packaged, PACKET_SIZE);
    bzero(buffer, PACKET_SIZE-5);
    
    int nread = fread(buffer, 1, PACKET_SIZE-5, fp);
    sprintf(packaged, "%04d", friend_fd);
    if(nread > 0)
    {
      strcat(packaged, buffer);
      //printf(".\n");
      write(socket_fd, packaged, strlen(packaged));
      nanosleep(&ts, NULL);
    }    
    if (nread < PACKET_SIZE-5)
    {
      if (feof(fp))
      {
      bzero(packaged, PACKET_SIZE);
      bzero(buffer, PACKET_SIZE-5);
      sprintf(packaged, "%04d", friend_fd);
      sprintf(buffer, "/ENDF");
      strcat(packaged, buffer);
      write(socket_fd, packaged, strlen(packaged));
      nanosleep(&ts, NULL);
	printf("End of file\n");
	return 0;
      }
      if (ferror(fp))
      {
	perror("Error reading\n");
	// TODO better handling. Should send special message?
	return 1;
      }
      else
      {
	perror("Unknown error.");
	return 2;
      }
      break;

    }  
  }
}

void *read_chat(void *socket)
{
  int n;
  char chat_buffer[PACKET_SIZE];
  int * socket_fd = (int *)socket;
  while(1){
    //read server response
    bzero(chat_buffer, PACKET_SIZE);
    n = read((* socket_fd), chat_buffer, PACKET_SIZE-1);
    if(n < 0){
      sleep(1); //sleep some time while waiting for a message
    } else {
      //display message
      //if(strlen(chat_buffer) < 2){
  if(friend_fd == 0 && strchr(chat_buffer, '|') != NULL){
	int index = strchr(chat_buffer, '|') - chat_buffer + 1;

	char fd_id [5];
	char stripped_message [PACKET_SIZE-5];
	memcpy(fd_id, &chat_buffer[0], n);
	memcpy(stripped_message, &chat_buffer[index], n - index -1);
	fd_id[index] = '\0';

	friend_fd = atoi(fd_id);
	strcpy(friend_nick, stripped_message);
	       //	friend_nick = stripped_message;
	
	printf("You are chatting with %s\n", stripped_message);
      }
      else if (0)
      {
	perror("How are you here?\n");
      }
      else if (strstr(chat_buffer, "/ERR") != NULL)
      {
      	printf("%s\n", &chat_buffer[4]);
	printf("The client will now close.\n");
	exit(0);
      }
      // Chatroom was closed
      else if (strstr(chat_buffer, "/PART") != NULL)
      {
      	friend_fd = 0;
	memset(friend_nick,0,sizeof(friend_nick));
      	printf("%s\n", &chat_buffer[5]);
      }
      // Hack to prevent triggering file creation, should compare indices of strings
      else if (strstr(chat_buffer, "/HELP") != NULL)
      {
	printf("%s: %s\n", friend_nick, chat_buffer);
      }
     else if (strstr(chat_buffer, "/FILE") != NULL)
      {
        char* filename = strrchr(chat_buffer, '/');
        filename++;

        receivingFile = 1;
        file = fopen(filename, "wb");
        printf("created file\n");

      }
      else if (strstr(chat_buffer, "/ENDF") != NULL){
          receivingFile = 0;
          fclose(file);
          printf("File transfer Complete\n");
      }
      else if(receivingFile == 1)
      {
        //printf("sent file chunck.\n");
        fwrite(chat_buffer, 1, strlen(chat_buffer), file);
        nanosleep(&ts, NULL);
      }
      else
      {
	printf("%s: %s\n", friend_nick, chat_buffer);
      }
    }
  }		
}

int main(int argc, char *argv[]){
  ts.tv_sec = 1 / 1000;
  ts.tv_nsec = (1 % 1000) * 1000000;
  
  int socket_fd, port_num, n;
  struct sockaddr_in server_addr;
  struct hostent * server;
  char packaged[PACKET_SIZE];
  char buffer[PACKET_SIZE-5];
  
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
  bzero(packaged, PACKET_SIZE-1);
  bzero(buffer, PACKET_SIZE-5);
  fgets(buffer, PACKET_SIZE-6, stdin);
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

  printf("Please enter your messages: ");
  while(1){
    
    //create message for server
    bzero(packaged, PACKET_SIZE-1);
    bzero(buffer, PACKET_SIZE-5);
    fgets(buffer, PACKET_SIZE-6, stdin);

    sprintf(packaged, "%04d", friend_fd);  

    // Check if file-transfer has started.
    if(strstr(buffer, "/FILE") == buffer)
    {
      buffer[strcspn(buffer, "\r\n")] = 0; // Trim newlines
      char filename [PACKET_SIZE-12];
      memcpy(filename, &buffer[6], PACKET_SIZE-12);
      int size;
      struct stat st;
      stat(filename, &st);
      size = st.st_size;

      if(size > SIZE_FILE_MAX)
      {
	printf("File size exceeded.");
	continue;
      }

      //printf("File %s, Size is %d\n", filename, size);
      strcat(packaged, "/FILE/");
      strcat(packaged, filename);
      
      //send message to server
      n = write(socket_fd, packaged, strlen(packaged));
      nanosleep(&ts, NULL);
      if(n < 0)
      { perror("Error writing to server"); exit(1); }

      transfer_file(socket_fd, filename);
    }

    // Majority case
    else
    {
      strcat(packaged, buffer);
      //send message to server
      n = write(socket_fd, packaged, strlen(packaged));
      nanosleep(&ts, NULL);
      if(n < 0)
      { perror("Error writing to server"); exit(1); }
    }
  }	
  close(socket_fd);
}
