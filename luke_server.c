#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 1004

// Throw an error.
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// Structure of client users.
typedef struct _USR {
	int clisockfd;		// socket file descriptor
	struct _USR* next;	// for linked list queue
} USR;

// Head and tail of the list.
USR *head = NULL;
USR *tail = NULL;

// Add a new client to the list.
void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

// Print the list of clients connected to server.
void print_client_list() {
	USR *temp = head;
	printf("Client list:\n");
	while (temp != NULL) {
		// FIXME: print their user names if possible.
		printf("\t%d\n", temp->clisockfd);
		temp = temp->next;
	}
}

// Broadcast the message from one client to all other clients.
void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) 
		error("ERROR Unknown sender!");

	// traverse through all connected clients
	char buffer[512];
	USR* cur = head;
	while (cur != NULL) {
		printf("In while\n");
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			printf("Found a cur who did not send message\n");
			
			memset(buffer, 0, 512);

			// prepare message
			sprintf(buffer, "[%s]:%s", inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			printf("Nsen: %d\n", nsen);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

// FIXME: what is this for?
typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

// Listen for messages from clients amongst other tasks.
void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	nrcv = recv(clisockfd, buffer, 255, 0);

	// If the user enters a blank character, remove from list.
	if (buffer[0] == 0 || buffer[0] == '\n' || buffer[0] == '\0') {
		printf("Client should be deleted\n%d", clisockfd);
		//FIXME: implement function to remove the userName.
	}

	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd); // add this new client to the client list
		
		// Print client list
		print_client_list();

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

