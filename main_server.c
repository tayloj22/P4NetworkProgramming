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

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	char* username;     // client username
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

void add_tail(int newclisockfd, char* newname)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->username = (char*) malloc(strlen(newname) * sizeof(char));
		strcpy(head->username, newname);
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->username = (char*) malloc(strlen(newname) * sizeof(char));
		strcpy(tail->next->username, newname);
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
	printf("[File Descriptor] : [username]\n");
	USR* cur = head;
	while (cur != NULL) {
		printf("\t[%d] : [%s]\n", cur->clisockfd, cur->username);
		cur = cur->next;
	}
	printf("-------------------------------------------------------------------------\n");
	
	return;
}

// LUKE
// Call function to delete client from list and print new client list.
void delete_routine(int clisockfd) {
	delete_client(clisockfd);
	printf("A client chose to disconnect.\n");
	print_list();
}

void broadcast(int fromfd,  char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// find the username associated with the sender
	char* username;
	int userfound = 0;
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is the one who sent the message
		if (cur->clisockfd == fromfd) {
			// set username if found
			username = cur->username;
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
			sprintf(buffer, "%s [%s]:%s", username, inet_ntoa(cliaddr.sin_addr), message);
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
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	char* username = ((ThreadArgs*) args)->username;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer);

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

		// Receive the username from the client, then create a LL entry
		//     with its sockfd and username
		char username[16];
		memset(username, 0, 16);
		int nrcv = recv(newsockfd, username, 16, 0);
		if (nrcv < 0) {
			error("ERROR recv() failed");
		}
		// add this new client to the client list
		add_tail(newsockfd, username);

		// print the new list of clients now that another has been added
		print_list();

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;
		args->username = username;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

