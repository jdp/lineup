include $(GOROOT)/src/Make.$(GOARCH)

.SUFFIXES: .go .$O

SRC = msgqueue.go server.go main.go
OBJ = ${SRC:.go=.$O}
OUT = lineupd

$(OUT): $(OBJ)
	mkdir -p bin
	$(LD) -o bin/$@ $(OBJ)

.go.$O:
	$(GC) $<

clean:
	rm -f $(OBJ) bin/$(OUT)

.PHONY: clean
