SRC = lineup.c message.c server.c
OBJ = ${SRC:.c=.o}
CC = gcc
CFLAGS = -ansi -pedantic -Wall
LIBS = -levent
OUT = lineupd

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm *.o
	rm $(OUT)
	
PHONY: clean

