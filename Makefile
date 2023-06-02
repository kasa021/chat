all: server client

server:server.c
	gcc -o server server.c -lpthread

client:client.c
	gcc -o client client.c -lpthread

runServer:
	./server 50000

runClient:
	./client localhost 50000

clean:
	rm -f  *~

clean-all:
	rm -f server client
