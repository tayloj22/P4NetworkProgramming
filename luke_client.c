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

#define PORT_NUM 1004

// Raise Error.
void error(const char *msg) {
	perror(msg);
	exit(0);
}

// Structure for the threead arguments.
typedef struct _ThreadArgs {
	int clisockfd;     /* The descriptor for the client */
	char *userName;    /* The username for the client */
} ThreadArgs;

// Main destination of the thread for recieving messages.
void* thread_main_recv(void* args)
{
	// Thread will release resources automatically, no need for join.
	pthread_detach(pthread_self());
	
	// Get the clisockfd from argument of function and release memory.
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	char *name = ((ThreadArgs*) args)->userName; //FIXME
	free(args);

	// Recieve a message from sockfd, the server.
	char buffer[512];
	int n;
	memset(buffer, 0, 512);
	n = recv(sockfd, buffer, 512, 0);
	printf("Client.thread_main_recv: nrecv=%d", n);
	printf("buffer=%s\n", buffer);
	
	// If an error did not occurr with the reciept, recieve again.
	while (n > 0) {
		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");
		printf("Client.thread_main_recv: nrecv=%d", n);
		printf("buffer=%s\n", buffer);
	}

	return NULL;
}

void* thread_main_send(void* args)
{
	// Thread will release resources automatically, no need for join.
	pthread_detach(pthread_self());
	
	// Get the clisockfd from argument of function and release memory.
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	char *name = ((ThreadArgs*) args)->userName; //FIXME
	free(args);

	char message[256];
	char buffer[256 - 16];
	int n;
	
	printf("Client.thread_main_send: username=%s\n", name);
	
	// Send messages to sockfd, the server, until an error occurs.
	while (1) {

		// Get message from user.
		printf("\nPlease enter the message: ");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);
		if (strlen(buffer) == 1) buffer[0] = '\0';

		// First 16 characters of message reserved for user name;
		for (int i = 0; i < 256; i++) 
			message[i] = i< 16 ? name[i] : buffer[i - 16];

		printf("Client.thread_main_send: message=%s\n", message);
		
		// Send the message, returns -1 for error else # chars sent.
		n = send(sockfd, message, strlen(message), 0);
		if (n < 0) error("ERROR writing to socket");
		// Break loop if user presses enter (chars sent == 0).
		if (n == 0) break; //FIXME
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	// Error for improper command line arguments.
	if (argc < 2) error("Please speicify hostname");
	// Get an endpoint for communication.
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	// Configure the socket communication line.
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);
	
	// Printing update on main window.
	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));
	
	// Connect socket to address space of serv_addr.
	int status = connect(sockfd, (struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	// User name from user, attached to thread args below.
	char nameInput[20];
	printf("Enter a (max) twenty character user name for client:\n");
	memset(nameInput, 0, 20);
	fgets(nameInput, 19, stdin);

	// Threads and thread arguments.
	pthread_t tid1, tid2;
	ThreadArgs* args;
	
	// Establish the arguments and create the thread for sending.
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->userName = nameInput; // FIXME
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	// Establish the arguments and create the thread for recieving.
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->userName = nameInput; // FIXME
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// Parent will wait for sender to finish, ie user stop sending 
		// messages and disconnect from server.
	pthread_join(tid1, NULL);

	// Close the connection.
	close(sockfd);

	return 0;
}

