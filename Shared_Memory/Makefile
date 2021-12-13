all : msg_server msg_client shm_server shm_client

msg_server : msg_server.c
		gcc msg_server.c -o msg_server -lpthread
msg_client : msg_client.c
		gcc msg_client.c -o msg_client -lpthread
shm_server : shm_server.c
		gcc shm_server.c -o shm_server -lpthread
shm_client : shm_client.c
		gcc shm_client.c -o shm_client -lpthread
	
clean : 
	rm msg_server
	rm msg_client
	rm shm_server
	rm shm_client
