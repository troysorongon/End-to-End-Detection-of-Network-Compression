PROG = client server standalone
all : $(PROGS)

client: client.c mjson.h
		gcc -g -o client client.c mjson.o

server: server.c mjson.h
		gcc -g -o server server.c mjson.o

standalone: standalone.c mjson.h
		gcc -g -o standalone standalone.c mjson.o

clean:
	rm -rf $(PROG)
