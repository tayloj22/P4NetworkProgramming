#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>
#include <ctype.h>

#define PORT_NUM 1004
#define CHAT_ROOM_NUM 5

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

// LUKE
// When user enters 'new' for the third command line
	// arguments, this function will work in tandum with
	// main_server.listen_for_chat room to join a new chat room
	// and print whether the join was successful or not.
void request_new_room(int sockfd) 
{
	// Send the message requesting a new room.
		// Will simply send one char: 'n'.
	char buffer[1];
	memset(buffer, 0, 1);
	buffer[0] = 'n';
	int n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR writing to socket");

	// Receieve the chat room number back or an'F' for failure.
	memset(buffer, 0, 1);
	n = recv(sockfd, buffer, 512, 0);
	if (n < 0) error("ERROR recv() failed");
	
	// Print the result of operation.
	printf("-------------------------------------------------\n");
	if (buffer[0] != 'F')
		printf("You have been assigned the chat room number %s\n", buffer);
	else
		printf("That chat room was full. Failure.\n");
	printf("-------------------------------------------------\n");	
}

// LUKE
// When user enters a room number for the third command line
	// arguments, this function will work in tandum with
	// main_server.listen_for_chat room to join the chat room
	// and print whether the join was successful or not.
void join_particular_room(int sockfd, int room_number) 
{
	// Send the message requesting to join a particular room.
		// Will simply send one char: the room number.
	char buffer[1];
	memset(buffer, 0, 1);
	buffer[0] = room_number + '0';
	int n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR writing to socket");
	
	// Recieve the room number back or a 'F' for failure.
	memset(buffer, 0, 1);
	n = recv(sockfd, buffer, 512, 0);
	if (n < 0) error("ERROR recv() failed");
	
	// Print the result of join operation.
	printf("-------------------------------------------\n");
	if (buffer[0] != 'F') 
		printf("You have been assigned the chat room number %s\n", buffer);
	else 
		printf("That chat room was full. Failure.\n");
	printf("-------------------------------------------\n");	
}

// LUKE
// When user does not enter anything for the third command line
	// arguments, this function will work in tandum with
	// main_server.listen_for_chat room to retrieve the available
	// chat rooms. It will then ask the user which room it wants to
	// join, or if the user would like to simply perform standard 
	// broadcast procedures. 
void retrieve_chat_rooms(int sockfd) 
{
	char buffer[1];
	int LEN = CHAT_ROOM_NUM * 10 + 1;
	char options[LEN];
	int n;

	// Send the 'l' to tell server to send the list.
	memset(buffer, 0, 1);
	buffer[0] = 'l';
	n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR writing to socket");
	
	// Get and the options from server.
	memset(options, 0, LEN);
	n = recv(sockfd, options, LEN, 0);
	if (n < 0) error("ERROR on recieving");
	
	// Print the options from server, and the user's choices.
	printf("-------------------------------------------\n");
	printf("This is the chatroom status (max six clients per room).\n");
	printf("Format is [Room Number] : [Num Clients]\n");
	printf("%s", options);
	printf("-------------------------------------------\n");
	printf("Enter a chat number to join it.\n");
	printf("Enter 'n' to create a new room.\n");
	printf("Enter 's' for standard broadcast to all clients.\n");
	
	// Get user input.
	memset(buffer, 0, 1);
	scanf("%c", &buffer[0]);		// Get input char.
	getchar();						// Consume the '\n'.

	// If user entered an 's', we send over the 's' to 
		// indicate standard broadcast procedures.
	if (buffer[0] == 's') {
		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");
		printf("Selected standard broadcast to all clients.\n");
	}
	// If the user entered an 'n', we request a new room.
	else if (buffer[0] == 'n') {
		request_new_room(sockfd);
	}
	// If the user entered an int, we request to join that room number.
	else if (isdigit(buffer[0]) != 0){
		join_particular_room(sockfd, atoi(&buffer[0]));
	}
	else { printf("Invalid Input\n"); }
}

// LUKE
// Main function to make intelligent decisions based upon the third
	// command line arguments. There are three paths to take:
		// 1. User enters no third argument: retrieve_chat_room
		// 2. User enters 'new': request_new_room
		// 3. User enters [int]: join_particular_room
void address_chat_room(char *arg3, int sockfd) 
{
	sleep(1);	// Wait for the server to catch up to client.

	// Call worker functions based upon contents are third arg.
	
	if (arg3 == NULL) { // No third arg.
		retrieve_chat_rooms(sockfd);
	}
	else if (strcmp(arg3, "new") == 0) { // 'new' is third arg.
		request_new_room(sockfd);
	}
	else { // Third arg is numeric.
		for (int i = 0; i < strlen(arg3); i++) {
			if (isdigit(arg3[i]) == 0) { 
				error("Invalid third argument");	
			}
		}
		join_particular_room(sockfd, atoi(arg3));
	}
}

void* thread_main_recv(void* args)
{
	//pthread_detach(pthread_self());
	//printf("Oficially in receiving message mode.\n");
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[512];
	int n;
	
	// Client will continue receiving messages until
	// the message is empty; it will then return
	do {
		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");
		
		// LUKE2
		printf("\n%s\n", buffer);
		printf("\nYou may now enter a message to the other users. ");
		printf("Blank message will quit.\n");
	
	} while (n > 0);

	return NULL;
}

void* thread_main_send(void* args)
{
	//pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[256];
	int n;

	while (1) {
		// The user will receive a prompt saying that they can enter a message
		// The message is sent to the server, where it is broadcasted
		printf("\nYou may now enter a message to the other users. ");
		printf("Sending a blank message will quit.\n");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);
		fflush(stdin);

		if (strlen(buffer) == 1) buffer[0] = '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

		// If the user types an empty string, strop transmission
		if (n == 0) break;
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please speicify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));
	// Client will now connect to the server
	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	// Ask the user to provide a username
	char userinput[17];
	char username[16];
	printf("Provide a username to be identified as (max 15 characters).\n");
	fgets(userinput, 17, stdin);
	fflush(stdin);
	for (int i = 0; i < strlen(userinput); i++) {
		if (userinput[i] == '\n') {
			userinput[i] = '\0';
		}
	}
	if (strlen(userinput) > 15) {
		printf("ERROR: username too long.\n");
		return -1;
	}
	strncpy(username, userinput, 16);

	// Send the username to the server to be stored
	int n = send(sockfd, username, strlen(username), 0);
	if (n < 0) { error("ERROR writing to socket"); }

	// LUKE
	// Complex function to address all chat room needs.
		// Please read function documentation above.
	address_chat_room(argv[2], sockfd);

	// Split into two threads
	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;

	// This thread will go to the send method
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	// This thread will go to the receive method
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish 
		// user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
	// LUKE
	//pthread_join(tid2, NULL);

	close(sockfd);

	return 0;
}

