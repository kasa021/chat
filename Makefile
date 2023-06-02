all: server client

server:server.c
	gcc -o server server.c -lpthread

client:client.c
	gcc -o client client.c -lpthread

runServer:
	./server 50001

runClient:
	./client localhost 50001

clean:
	rm -f  *~

clean-all:
	rm -f server client
