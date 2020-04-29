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

// Raise error.
void error(const char *msg) {
	perror(msg);
	exit(1);
}

// Structure of client users.
typedef struct _USR {
	int clisockfd;		// socket file descriptor.
	char *userName;     // username of clients.
	struct _USR* next;	// for linked list queue.
} USR;

// Head and tail of the list.
USR *head = NULL;
USR *tail = NULL;

// Add a new client to the list, from its socket descriptor and name.
void add_tail(int newclisockfd, char *newUserName)
{
	if (head == NULL) { // If list is empty, add to head.
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->userName = newUserName;
		head->next = NULL;
		tail = head;
	} 
	else { // Otherwise, add to tail.
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		head->userName = newUserName;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

// Remove a client from the list. // FIXME: not tested.
void remove_client(int clisockfd) {
	USR *temp = head;
	while (temp != NULL) {
		if (temp->next->clisockfd == clisockfd) {
			USR *u = temp->next->next;
			free(temp->next);
			temp->next = u;
			break;
		}
		temp = temp->next;
	}
}

// Print the list of clients connected to server.
void print_client_list() {
	USR *temp = head;
	printf("Client list:\n");
	while (temp != NULL) {
		printf("\t%s: %d\n", temp->userName, temp->clisockfd);
		temp = temp->next;
	}
}

// Broadcast the message from one client to all other clients.
void broadcast(int fromfd, char* message, char *fromName)
{
	// Get the address of the sender.
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) 
		error("ERROR Unknown sender!");

	char buffer[512];				// Full buffer to send.
	char send_message[512 - 16];	// Actual message.
	USR* cur = head;

	// Traverse through list of all connected clients.
	while (cur != NULL) {

		// If current client is not the send, broadcast message.
		if (cur->clisockfd != fromfd) {
			
			// Prepare the message.
			memset(buffer, 0, 512);
			// First 16 characters of message reserved for user name.
			for (int i = 0; i < 512; i++) // FIXME: message out bounds
				send_message[i] = i < 16 ? fromName[i] : message[i-16];
			sprintf(buffer, "[%s]:%s", 
					inet_ntoa(cliaddr.sin_addr), send_message);	

			// Send the message.
			int len_send = strlen(buffer);
			int nsen = send(cur->clisockfd, buffer, len_send, 0);
			if (nsen != len_send) 
				error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

// Structure to define the arguments given to thread.
typedef struct _ThreadArgs {
	int clisockfd;
	char *userName;
} ThreadArgs;

// Listen for messages from clients amongst other tasks.
void* thread_main(void* args)
{
	// Make sure thread resources are deallocated upon return.
	pthread_detach(pthread_self());

	// Get socket descriptor from argument then free.
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	char *userName = ((ThreadArgs*) args)->userName;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	nrcv = recv(clisockfd, buffer, 255, 0);

	// If the user enters a blank character, remove from list.
	if (buffer[0] == 0 || buffer[0] == '\n' || buffer[0] == '\0') {
		printf("Client deleted:%d\n", clisockfd);
		remove_client(clisockfd); // FIXME: not tested.
		print_client_list();
	}

	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, buffer, userName);

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
		// add this new client to the client list
		add_tail(newsockfd, "FIXME"); 
		// FIXME: somehow we need to add the user name into add_tail.
		
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

