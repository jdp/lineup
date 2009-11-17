include $(GOROOT)/src/Make.$(GOARCH)

.SUFFIXES: .go .$O

SRC = msgqueue.go server.go main.go
OBJ = ${SRC:.go=.$O}
OUT = lineupd

$(OUT): $(OBJ)
	$(LD) -o $@ $(OBJ)

.go.$O:
	$(GC) $<

clean:
	rm -f $(OBJ) $(OUT)

.PHONY: clean
