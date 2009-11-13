SRC = lineup.c server.c message.c scripting.c
OBJ = ${SRC:.c=.o}
CC = gcc
CFLAGS = -ansi -pedantic -Wall
LLUA = -llua5.1 # some systems need lua version. -llua might work
LIBS = -levent $(LLUA)
OUT = lineup

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c $<
	
clean:
	rm *.o
	rm $(OUT)
	
PHONY: clean

