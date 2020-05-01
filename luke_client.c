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
#include <errno.h>

#define PORT_NUM 1004

char myusername[16];	// This client's local user name.

// Raise an error.
void error(const char *msg)
{
	perror(msg);
	exit(0);
}

// Structure for thread arguments.
typedef struct _ThreadArgs {
	int clisockfd;
	char* name;
} ThreadArgs;

// Handle recieving a message.
void* thread_main_recv(void* args)
{
	// Ensure resource deallocation upon termination.
	pthread_detach(pthread_self());

	// Get the sockfd from arguments.
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[512];		// Memory space for message and sender.
	char message[512 - 16];	// Memory for the message.
	char sender[16];		// Sender of message.
	int n = 1;				// Number of characters recieved.

	// Receive and display message from server.
	while (n > 0) {

		// Clear buffer, sender and message.
		memset(buffer, 0, 512);
		memset(message, 0, 512 - 16);
		memset(sender, 0, 16);

		// Recieve the raw message into buffer.
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) {
			printf("%d\n", errno);
			error("ERROR recv() failed");
		}
		
		// Extract the message from the buffer.
		strcpy(message, buffer + 16);
		
		// Print the message and the sender.
		printf("\nClient has receieved a message.");
		printf("\n\tRaw Buffer: %s", buffer);
		printf("\n\tUsername: ");
		int i = 0;
		for (i = 0; i < strlen(buffer); i++) {
			if (buffer[i] == ':') {
				printf("\n\tMessage: ");
			}
			else {
				printf("%c", buffer[i]);
			}
		}
		printf("\n");	
	}

	return NULL;
}

void* thread_main_send(void* args)
{
	// Ensure resource deallocation upon termination.
	pthread_detach(pthread_self());
	
	// Create variables from the passed ThreadArgs, then free them.
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[256];		// Memory for username + message to send.
	char message[256 - 16 - 1];	// Memory for the actual message.
	char username[16];		// Copy of myusername.
	int n;					// Number of characters sent/recieved.

	// Use a copy of myusername for all of this stuff.
	strcpy(username, myusername);

	// Fill first strlen(username) indices of buffer with username.
	for (int i = 0; i < strlen(username); i++)
		buffer[i] = myusername[i];
	// Add a colon after the username
	buffer[strlen(username)] = ':';

	while (1) {
		// Always clear the last bits of buffer and message.
		memset(buffer + 16, 0, 256 - 16);
		memset(message, 0, 256 - 16);
			
		// Get the message.
		printf("Please enter the message: ");
		fgets(message, 255 - 16 - 1, stdin);
		strtok(message, "\n");

		// Add the message to the buffer.
		for (int i = 0; i < strlen(message); i++) 
			buffer[i + strlen(username) + 1] = message[i];
	
		// Testing: printing the buffer
		printf("Everything I'm going to send: ");
		for (int i = 0; i < strlen(buffer); i++)
			printf("%c", buffer[i]);
		printf("\n");
		
		// Stop transmission once the user presses enter with no input.
		if (strlen(message) == 1) break; 
		
		// Send the buffer.
		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	// Get the host address from user.
	if (argc < 2) error("Please speicify hostname");
	
	// Create the socket for communication.
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	// Configure the socket.
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);
	
	// Print a little update.
	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));
	int status = connect(sockfd, (struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");
	
	// Get username from user.
	char userinput[16];
	printf("Enter username, max 15 characters.\n");
	fgets(userinput, 17, stdin);
	strtok(userinput, "\n");
	strcpy(myusername, userinput);
	fflush(stdin);

	pthread_t tid1;		// Thread for sending messages.
	pthread_t tid2;		// Thread for recieving messages.
	ThreadArgs* args;	// Arguments to pass into threads.
	
	// Launch thread to send messages.
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	// Launch thread to recieve messages.
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// Wait for sender to finish, ie user stops sending or disconnect.
	pthread_join(tid1, NULL);

	// Close communication port.
	close(sockfd);

	return 0;
}
