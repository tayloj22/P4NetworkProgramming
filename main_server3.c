#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define PORT_NUM 1004

#define RESET "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1b[34m"
#define KMAG "\x1b[35m"
#define KCYN "\x1b[36m"
#define KWHT "\x1B[37m"

// LUKE
#define CHAT_ROOM_NUM 5
#define CLIENTS_PER_ROOM 6

// LUKE
// Each index hold the number of clients in that chat room.
int chatroom[CHAT_ROOM_NUM];

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char* username;     // client username
	char* color;	    // client color
	struct _USR* next;	// for linked list queue
	// LUKE
	int chatroom;	// -1 if not in a room, else the room number.
} USR;

USR *head = NULL;
USR *tail = NULL;

void add_tail(int newclisockfd, char* newname, char* newcolor, int chat_status)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		// LUKE
		head->chatroom = chat_status;

		head->username = (char*) malloc(strlen(newname) * sizeof(char));
		strcpy(head->username, newname);

		head->color = (char*) malloc(strlen(newcolor) * sizeof(char));
		strcpy(head->color, newcolor);
		
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		// LUKE
		tail->next->chatroom = chat_status;
		
		tail->next->username = (char*) malloc(strlen(newname) * sizeof(char));
		strcpy(tail->next->username, newname);

		tail->next->color = (char*) malloc(strlen(newcolor) * sizeof(char));
		strcpy(tail->next->color, newcolor);

		tail->next->next = NULL;
		tail = tail->next;
	}
}

// LUKE
// Delete a client from the list and return its nickname.
void delete_client(int clisockfd) 
{	
	USR *temp = head;
	USR *prev;
	// Delete from head.
	if (temp != NULL && temp->clisockfd == clisockfd) {
		head = temp->next;
		free(temp);
		return;
	}
	// Locate from body.
	while (temp != NULL && temp->clisockfd != clisockfd) {
		prev = temp;
		temp = temp->next;
	}
	// Delete if found, modify tail if needed.
	if (temp == NULL) return;
	prev->next = temp->next;
	if (temp == tail) tail = prev;
	free(temp);
	return;
}

// LUKE
// Jack, I moved ur function up here because I needed to be declared
	// higher up and I also slightly modified the formatting of output.
void print_list() {
	printf("-------------------------------------------------------------------------\n");
	printf("All currently connected clients are listed below:\n");
	printf("Format is [FD] : [username] : [chatroom]\n");
	USR* cur = head;
	while (cur != NULL) {
		printf("\t[%d] : [%s] : [%d]\n", cur->clisockfd, cur->username, cur->chatroom);
		cur = cur->next;
	}
	printf("-------------------------------------------------------------------------\n");
	
	return;
}

// LUKE
// Call function to delete client from list and print new client list.
void delete_routine(int clisockfd) {
	delete_client(clisockfd);
	print_list();
}

// Broadcast a message to ALL clients.
void broadcast(int fromfd,  char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// find the username and color associated with the sender
	char* username;
	char* color;
	int userfound = 0;
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is the one who sent the message
		if (cur->clisockfd == fromfd) {
			// set username if found
			username = cur->username;
			color = cur->color;
			userfound = 1;
		}
		cur = cur->next;
	}
	if (userfound == 0) {
		printf("Sender username not found.\n");
	}

	// traverse through all connected clients
	cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "%s%s [%s]:%s" RESET, color, username, inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
	char* username;
	char* color;
	// LUKE
	int chat_status;
} ThreadArgs;

// LUKE
// Search for an empty chatroom and return its number. 
	// Return -1 if all full.
int create_new_chatroom() 
{
	// Search for empty chatroom.
	int i = 0;
	while (i < CHAT_ROOM_NUM && chatroom[i] != 0) i++;
	// Return -1 if no empty room exists.
	if (i == CHAT_ROOM_NUM) 
		return -1;
	// Otherwise, increment the number of clients and return.
	chatroom[i]++;
	return i;
}

// Join a chat room and return the room joined on success. 
	// Return -1 if the room is full.
int join_chatroom(int room)
{
	if (chatroom[room] < CLIENTS_PER_ROOM) {
		chatroom[room]++;
		return room;
	}
	else return -1;
}

// LUKE
// Broadcast a message ONLY within a particular chatroom.
void chat_room_broadcast(int fromfd, char *message, int room) 
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// find the username and color associated with the sender
	char* username;
	char* color;
	int userfound = 0;
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is the one who sent the message
		if (cur->clisockfd == fromfd) {
			// set username if found
			username = cur->username;
			color = cur->color;
			userfound = 1;
		}
		cur = cur->next;
	}
	if (userfound == 0) {
		printf("Sender username not found.\n");
	}

	// traverse through all connected clients
	cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
			// and is in the correct chatroom.
		if (cur->clisockfd != fromfd && cur->chatroom == room) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "%s[room %d] : [%s] : [%s] : %s" RESET, color, room, username, inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

// Format the available chatrooms into a nice string.
char get_available_rooms(char *available) 
{
	// Every line looks like: "[room] : [num]\n".
	for (int i = 0; i < CHAT_ROOM_NUM; i++) {
		available[0 + i * 10] = '[';
		available[1 + i * 10] = i + '0';
		available[2 + i * 10] = ']';
		available[3 + i * 10] = ' ';
		available[4 + i * 10] = ':';
		available[5 + i * 10] = ' ';
		available[6 + i * 10] = '[';
		available[7 + i * 10] = chatroom[i] + '0';
		available[8 + i * 10] = ']';
		available[9 + i * 10] = '\n';
	}
	// Important to add string termination character.
	available[CHAT_ROOM_NUM * 10] = '\0';
}

// LUKE
// Complex communication procedure with client to determine chatroom.
	// Returns -1 if no chatroom is requested. This occurs if the client 
		// sends over the 's' character (standard broadcast procedure). 
	// Otherwise returns the chat room number that this client should engage with.
		// This occurs if the client send over a 'n' (create a new chatroom)
		// or an chatroom number.
int listen_for_chat_room(int clisockfd) 
{
	int nsen;									// Number of chars sent.
	int nrcv;									// Number of chars recieved.
	int temp;									// Temporary storage.
	char chatroom[1];							// Send/recieve 1 char.
	char available[CHAT_ROOM_NUM * 10 + 1];		// Send available chatrooms.
	
	// Recieve the chat room request option from the client.
		// Comes in form of an 'l', 's', 'n' or integer.
	memset(chatroom, 0, 1);
	nrcv = recv(clisockfd, chatroom, 1, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	// If option is an 'l', we send the list of available chatrooms back.
		// Then we listen again, for either an 's', 'n', or integer
		// and conduct our response based upon that below.
	if (chatroom[0] == 'l') {
		get_available_rooms(available);
		nsen = send(clisockfd, available, strlen(available), 0);
		if (nsen < 0) error("ERROR send() failed");
		memset(chatroom, 0, 1);
		nrcv = recv(clisockfd, chatroom, 1, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	// If option is an 's', return -1 for standard broadcast procedures.
	if (chatroom[0] == 's') 
		return -1;

	// If option is an 'n', create a new chatroom and return its number.
		// If the operation fails, will send an 'F' back to client, 
			// otherwise will send the room number.
	if (chatroom[0] == 'n') {
		temp = create_new_chatroom();
		chatroom[0] = temp == -1 ? 'F' : temp + '0';
		nsen = send(clisockfd, chatroom, 1, 0);
		if (nsen < 0) error("ERROR send() failed");
	}

	// If option is an int, join the specified chatroom and return its number.
		// If the operation fails, will send an 'F' back to client, 
			// otherwise will send the room number.
	else if (isdigit(chatroom[0]) != 0) { 
		temp = join_chatroom(atoi(&chatroom[0]));
		chatroom[0] = temp == -1 ? 'F' : temp + '0';
		nsen = send(clisockfd, chatroom, 1, 0);
		if (nsen < 0) error("ERROR send() failed");
	}
	
	// Return -1 for failures.
	if (chatroom[0] == 'F') {
		printf("Chat room joining failed.\n");	
		return -1;
	}

	// Return the chatroom number for successfully joining.
	printf("-----------------------------------------------------\n");
	printf("Recieved a request for client, %d", clisockfd);
	printf(" to join the chatroom %s\n", chatroom);
	printf("-----------------------------------------------------\n");
	return atoi(&chatroom[0]);
}

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	char* username = ((ThreadArgs*) args)->username;
	char* color = ((ThreadArgs*) args)->color;
	// LUKE
	// Perform the broadcast routine dictated by chat_status:
		// -1 for broadcast to all, else a chatroom number.
	int chat_status = ((ThreadArgs*) args)->chat_status;
	free(args);	

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// LUKE
		// Broadcast to all or broadcast to a particular chatroom.
		if (chat_status == -1)
			broadcast(clisockfd, buffer);
		else
			chat_room_broadcast(clisockfd, buffer, chat_status);

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	// LUKE
	// Delete the client from the list and print.
	delete_routine(clisockfd);

	close(clisockfd);
	//-------------------------------

	return NULL;
}


int main(int argc, char *argv[])
{
	// Initialize all chatrooms to be empty.
	for (int i = 0; i < CHAT_ROOM_NUM; i++)
		chatroom[i] = 0;

	// Create char* array of all possible color choices
	char* colorcodes[] = {
		KRED,
		KGRN,
		KYEL,
		KBLU,
		KMAG,
		KCYN,
		KWHT,
	};
	int colorchoice = 0;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	// maximum number of connections = 5
	listen(sockfd, 5);

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		printf("Please wait while this new client connects.\n");

		// Receive the username from the client.
		char username[16];
		memset(username, 0, 16);
		int nrcv = recv(newsockfd, username, 16, 0);
		if (nrcv < 0) {
			error("ERROR recv() failed");
		}

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;
		args->username = username;
		args->color = colorcodes[colorchoice];
		
		// LUKE
		// Complex communication procedure with client to determine chatroom.
			// Gets a -1 for standard broadcast procedures, else a chatroom num.
			// Please read the function documentation above.
		int chat_status = listen_for_chat_room(newsockfd);
		args->chat_status = chat_status;
		
		// Add to the tail of list and print.
		add_tail(newsockfd, username, colorcodes[colorchoice % 7], chat_status);
		colorchoice++;
		print_list();

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

