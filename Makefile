CFLAGS = -g -std=gnu99 
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes -v
BIN = client server

all: $(BIN)

%.o:%.c
	gcc $(CFLAGS) -c $^ 

client: client.o pgmread.o linkedlist.o send_packet.o
	gcc $(CFLAGS) client.o pgmread.o linkedlist.o send_packet.o -o $@

server: server.o pgmread.o
	gcc $(CFLAGS) server.o pgmread.o -o $@

clean:
	rm -f $(BIN) *.o

check_client: client
	valgrind $(VFLAGS) ./client 127.0.0.1 2020 list_of_filenames.txt 20

check_server: server
	valgrind $(VFLAGS) ./server 2020 big_set output.txt

	


	