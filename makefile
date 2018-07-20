all: client server

client: client.o common.o
	cc -g -o client client.o common.o

server: server.o common.o
	cc -g -o server server.o common.o

client.o: client.c

server.o: server.c

common.o: common.c common.h

.PHONY: clean

clean:
	rm -f ./*.o client server