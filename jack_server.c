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

// Raise an error.
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// Struct of user objects.
typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char *username;		// user name of client
	struct _USR* next;	// for linked list queue
} USR;

// Head and tail of list of client users.
USR *head = NULL;
USR *tail = NULL;


// Function to add a client to the tail of the list.
void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} 
	else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

// Attatch the client's user name to the correct USR.
void attatch_username(int clisockfd, char *username) {
	USR *tmp = head;
	while (tmp != NULL) {
		if (tmp->clisockfd == clisockfd) {
			tmp->username = username;
			return;
		}
		tmp = tmp->next;
	}
}

// Delete a client from the list.
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
	if (temp == NULL) return;
	
	// Delete from body or tail.
	prev->next = temp->next;
	if (temp == tail) tail = prev;
	free(temp);
}

// Print the list of clients connected to server.
void print_client_list() {
	USR *tmp = head;
	printf("Client list:\n");
	while (tmp != NULL) {
		printf("\t%s: %d\n", tmp->username, tmp->clisockfd);
		tmp = tmp->next;
	}
}


void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	int foundNL = 0;
	for (int i = 0; i < strlen(message); i++) {
		if (message[i] == '\n') {
			foundNL = 1;
		}
		if (foundNL == 1) {
			message[i] = '\0';
		}
	}

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];
			memset(buffer, 0, 512);

			// prepare message
			fflush(stdout);
			fflush(stdin);
			sprintf(buffer, "[%s]:%s", inet_ntoa(cliaddr.sin_addr), message);
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
	char *userName;
} ThreadArgs;

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

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0;  
}

