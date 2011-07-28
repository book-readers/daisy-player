CC      = gcc
PREFIX  = /usr/local/
LD      = $(CC)
LDLIBS  = -lncursesw -lidn -lsox
OFILES  = daisy-player.o 

all: $(OFILES)
	$(LD) -Wall $(OFILES) $(LDLIBS) -o daisy-player

daisy-player.o: daisy-player.c
	$(CC) -Wall -D PREFIX=\"$(PREFIX)\" -c $< -o $@

clean:
	rm -f *.o daisy-player

install:                      
	@./install.sh $(PREFIX)
