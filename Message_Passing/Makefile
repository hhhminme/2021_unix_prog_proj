all : server client

server : msg_server.c
	gcc msg_server.c -o server -lpthread
client : msg_client.c
	gcc msg_client.c -o client -lpthread

clean :
	rm server
	rm client


