default: luke

luke:
	gcc -o luke_server luke_server.c -lpthread
	gcc -o luke_client luke_client.c -lpthread

yoon:
	gcc -o chat_server chat_server.c -lpthread
	gcc -o chat_client chat_client.c
	gcc -o chat_server_full chat_server_full.c -lpthread
	gcc -o chat_client_full chat_client_full.c -lpthread
	gcc -o jack_server jack_server.c -lpthread
	gcc -o jack_client jack_client.c -lpthread
	gcc -o luke_server luke_server.c -lpthread
	gcc -o luke_client luke_client.c -lpthread

jack:
	gcc -o jack_server jack_server.c -lpthread
	gcc -o jack_client jack_client.c -lpthread

clean:
	rm luke_server jack_client
	rm jack_server jack_client
	rm chat_server chat_client
	rm chat_server_full chat_client_full
	rm jack_server jack_client
	rm luke_server luke_client
