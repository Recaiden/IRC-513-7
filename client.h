#ifndef H_CLIENT
#define H_CLIENT

// Connect to TRS on port.  Wait for ACK and return success or failure
// Done automatically on startup
int cmd_connect(int sock, struct sockaddr_in server);

// Request to move from Q to Channel.  Wait for IN_SESSION
int cmd_chat(char* id_channel);

// Flag chatting partner.  Parameter may not be necessary.
int cmd_flag(char* id_channel);

// Ask for command listing and print them when received.
int cmd_help();
int cmd_quit();
int cmd_transfer();

// Send a message through the server to the other participant in the channel.
int cmd_send(int sock);

#endif
