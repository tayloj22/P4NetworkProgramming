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
	if (head == NULL) { // Add to head of list.
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} 
	else { //Add to tail of list.
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
	// Delete if found, modify tail if needed.
	if (temp == NULL) return;
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

// Broadcast a message from one client to all other clients.
	// fromfd -- clisockfd of sender client.
	// message -- message of sender client.
void broadcast(int fromfd, char* message)
{
	// Figure out sender address.
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) 
		error("ERROR Unknown sender!");

	// Traverse through all connected clients.
	USR* cur = head;
	while (cur != NULL) {

		if (cur->clisockfd != fromfd) { // Non-sender clients.
			// Prepare the message.
			char buffer[512];
			memset(buffer, 0, 512);
			for (int i = 0; i < strlen(message); i++) 
				buffer[i] = message[i];

			// Testing: print the message.
			printf("Server sending a message to a client:");
			for (int i = 0; i < strlen(buffer); i++)
				printf("%c", buffer[i]);
			printf(":\n");

			// Send the message.
			int nmsg = strlen(buffer);
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

// Arguments to pass to threads.
typedef struct _ThreadArgs {
	int clisockfd;
	char *userName;
} ThreadArgs;

// Listen to a particular client and call broadcast as needed.
	// args - ThreadArgs containing the clisockfd of the particular
		// client this thread is interacting with.
void* thread_main(void* args)
{
	// Make sure thread resources are deallocated upon return.
	pthread_detach(pthread_self());

	// Get socket descriptor from argument.
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[256];	// Memory for sending/recieving.
	int nsen, nrcv;		// Number of chars sent/recieved.

	// Recieve a message.
	nrcv = recv(clisockfd, buffer, 255, 0);
	printf("nrcv: %d\n", nrcv);
	if (nrcv < 0) error("ERROR recv() failed");
	
	printf("Server recieved the following message:");
	for (int i = 0; i < nrcv; i++) 
		printf("%c", buffer[i]);
	printf(": \n");

	// Broadcast the message and recieve another message.
	while (nrcv > 0) {
		broadcast(clisockfd, buffer);
		nrcv = recv(clisockfd, buffer, 255, 0);
		printf("nrcv: %d\n", nrcv);
		if (nrcv < 0) error("ERROR recv() failed");
	}
	
	// Close the connection with this client.
	close(clisockfd);

	return NULL;
}

int main(int argc, char *argv[])
{
	// Create the socket for communication.
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	
	// Configure the socket.
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	// Bind the socket to address space.
	int status = bind(sockfd, (struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");
	// This socket will be used to listen to a maximum of 5 clients.
	listen(sockfd, 5);

	// While server exists.
	while(1) {

		// Listen for and accept clients.
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
				(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		// Add client to tail of list and print list.
		printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd);
		print_client_list();

		// Prepare ThreadArgs structure to pass client socket.
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		args->clisockfd = newsockfd;
		
		// Create and launch the thread for communication with client.
		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0)
			error("ERROR creating a new thread");
	}

	return 0;  
}

