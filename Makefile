PROG = client
OBJS = client.o mjson.o
DEBUG = -g

$(PROG) : $(OBJS)
		gcc $(DEBUG) -o $(PROG) $^

%.o : %.c mjson.h
		gcc $(DEBUG) -c -o $@ $<

clean:
	rm -rf $(PROG) $(OBJS)
