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

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	//pthread_detach(pthread_self());

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

		printf("\n%s\n", buffer);
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
	if (n < 0) {
		error("ERROR writing to socket");
	}


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

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}

