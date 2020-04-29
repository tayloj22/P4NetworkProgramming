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
	char* name;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;

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
	pthread_detach(pthread_self());
	
	// create variables from the passed ThreadArgs, then free them
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	char userName[16];
	strcpy(userName, ((ThreadArgs*) args)->name);
	free(args);

	// keep sending messages to the server
	char buffer[256];
	char message[238];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		printf("\nPlease enter the message: ");
		memset(buffer, 0, 256);
		// create separate message buffer with user input
		memset(message, 0, 238);
		fgets(message, 237, stdin);
		// concatenate the username, a separator, and the message to create the buffer
		strcat(buffer, userName);
		strcat(buffer, ": ");
		strcat(buffer, message);

		// We stop transmission once the user presses enter with no input
		if (strlen(message) == 1) message[0] = '\0';
		if (message[0] == '\0') break;
		
		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");

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

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	// Prompt the user to create a username which will be used as args
	char userName[16];
	printf("Please create a username of max 16 characters.\n");
	fgets(userName, 16, stdin);
	// Strip the newline character
	strtok(userName, "\n");
	// Flush the standard input
	fflush(stdin);


	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->name = userName;
	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->name = userName;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}
