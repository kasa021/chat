all: server_async client

server_async:server_async.c
	gcc -o server_async server_async.c

client:client.c
	gcc -o client client.c

runServer:
	./server_async 50000

runClient:
	./client localhost 50000

clean:
	rm -f  *~

clean-all:
	rm -f server_async client
