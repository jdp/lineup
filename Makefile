SRC = lineup.c server.c message.c
OBJ = ${SRC:.c=.o}
CC = gcc
OUT = lineup
LIBS = -levent

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS)
	
.c.o:
	$(CC) -c $<
	
clean:
	rm *.o
	rm $(OUT)
	
PHONY: clean

