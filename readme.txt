# IRC-513-7
CS 513 Class Project 1 for Team 7, Omegle-like IRC client

Compiling the code:
Server:
1. To compile the server code navigate to the project folder using terminal.
2. Issue the command "make server" which will use the make file to create server.exe
3. To run the server type "./server.exe" into the terminal and a server instance will be created.
4. Once created type /START to start accepting clients for chatting.

Client:
1. 1. To compile the client code navigate to the project folder using terminal.
2. Issue the command "make client" which will use the make file to create client.exe
3. To run a client type "./client.exe hostname port" into the terminal and a client instance will be created as long as the server is running and accepting clients otherwise the connection will be refused.
4. If successful the program will ask for the user to choose a nickname.
5. After choosing a nickname the client can type the command "/CHAT" which will signal the server that the client wants to be added to the client chatting queue.
6. When there is another client in the chat client queue, the server will pair the user with anoter client user providing them with their nickname.
7. From here the client can chat by sending messages, or use specific commands defined below.

Client speific commands:
1. /FILE filepath - This call will take the specific file defined in the filepath and will send it to the partner client that is chatting with the user. When receiveing a file, the file will be created in the directory in which the client program is located.

2. /FLAG - This call will alert the server that a user wants to flag their partner which will be stored on the server and viewable by the admin of the server when using the /STATS command.

3. /QUIT - This call will end the communication of a client who is chatting with another client. Both clients will be alerted that they were disconnected.

4. /HELP - This call will give the user tips on what commands can be typed in that the server will know what to do with.

Server specific commands:
1. /STATS - This call gives the server admin a detailed printout about the current server queue, the 

2. /BLOCK fd - blocks the user of the fd given this will not allow them to connect to chat.

3. /UNBLOCK fd - unblocks the user of the fd given if they were originally block allowing them to now connect to chat.

4. /THROW fd - throws out a client from a chat and disconnects their partner from the chat as well.

5. /START - starts up the server to allow clients to connect

6. /END - ends the server and disconnects all users gracefully.
