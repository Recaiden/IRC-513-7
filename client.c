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
Because our chat system does not strictly follow a send/response protocol 
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

    if(nread > 0)
    {
      //printf("BUFFERING:%s\n", buffer);
      //sprintf(packaged, "%04d%s", friend_fd, buffer);

      //printf("SENDING: %s\n", buffer);
      write(socket_fd, buffer, strlen(buffer));
      nanosleep(&ts, NULL);
    }    
    if (nread < PACKET_SIZE-5)
    {
      if (feof(fp))
      {
      bzero(packaged, PACKET_SIZE);
      bzero(buffer, PACKET_SIZE-5);
      //sprintf(packaged, "%04d", friend_fd);
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
      printf("File transferred.");
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
    //printf("RECEIVED: %s\n", chat_buffer);
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
	memcpy(stripped_message, &chat_buffer[index], n - index);
	fd_id[index] = '\0';

	friend_fd = atoi(fd_id);
	strcpy(friend_nick, stripped_message);
	       //	friend_nick = stripped_message;
	
	printf("You are chatting with %s\n", stripped_message);
      }
      else if (strstr(chat_buffer, "/ENDF") != NULL){
          receivingFile = 0;
          fclose(file);
          printf("File transfer Complete\n");
      }
      else if(receivingFile == 1)
      {
	//printf("rcvd chunk %s\n", chat_buffer);
        fwrite(chat_buffer, 1, n, file);
	nanosleep(&ts, NULL);
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
        file = fopen(filename, "w+b");
        printf("created file\n");

      }
      else
      {
	if(strlen(chat_buffer) == 0)
	{
	  printf("The server is no longer available.  Closing client.\n");
	  if(receivingFile)
	    fclose(file);
	  exit(0);
	}
	printf("%s: %s\n", friend_nick, chat_buffer);
      }
    }
  }		
}

int main(int argc, char *argv[]){
  ts.tv_sec = 100 / 1000;
  ts.tv_nsec = (1 % 1000) * 1000000;
  
  int socket_fd, n;
  char packaged[PACKET_SIZE];
  char buffer[PACKET_SIZE-5];
  
  //not given hostname and port
  if(argc <3){
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    exit(0);
  }
  
  //init socket
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  
  if(socket_fd < 0){
    perror("Error opening socket");
    exit(1);
  }

  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
  hints.ai_socktype = SOCK_STREAM;
  
  if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }
  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((socket_fd = socket(p->ai_family, p->ai_socktype,
			 p->ai_protocol)) == -1)
    {
      perror("socket");
      continue;
    }

    if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(socket_fd);
      //perror("connect");
      continue;
    }
    
    break; // if we get here, we must have connected successfully
  }
  

  // Identifying Connect command
  printf("Please enter your nickname: ");
  //create message for server
  bzero(packaged, PACKET_SIZE-1);
  bzero(buffer, PACKET_SIZE-5);
  fgets(buffer, PACKET_SIZE-5, stdin);
  buffer[strcspn(buffer, "\r\n")] = 0; // Trim newlines
  sprintf(packaged, "/CONN");
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
    buffer[strcspn(buffer, "\r\n")] = 0; // Trim newlines

    //sprintf(packaged, "%04d", friend_fd);  

    // Check if file-transfer has started.
    if(strstr(buffer, "/FILE") == buffer)
    {
      if(friend_fd == 0)
      {
	printf("Cannot send file while in queue.\n");
	continue;
      }
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

      strcat(packaged, "/FILE/");
      strcat(packaged, filename);

      FILE *fp;
      fp = fopen(filename, "rb"); 
      if(fp == NULL)
      { 
	perror("Error opening file");
      }
      else
      {
	//send message to server
	n = write(socket_fd, packaged, strlen(packaged));
	nanosleep(&ts, NULL);
	if(n < 0)
	  { perror("Error writing to server"); continue; }

	transfer_file(socket_fd, filename);
      }
    }

    // Majority case
    else
    {
      //strcat(packaged, buffer);
      //send message to server
      n = write(socket_fd, buffer, strlen(buffer));
      nanosleep(&ts, NULL);
      if(n < 0)
      { perror("Error writing to server"); exit(1); }
    }
  }	
  close(socket_fd);
}
