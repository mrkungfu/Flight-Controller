CC=gcc 
main: server client

client: client.o
server: server.o

clean:
	rm -f server.o client.o
	
clean_all:
	rm -f server client server.o client.o
