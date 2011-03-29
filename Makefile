CC      = gcc
PREFIX  = /usr/local/
LD      = $(CC)
LDLIBS  = -lncursesw -lcdio -lidn -lsox -lm
OFILES  = daisy-player.o

all: $(OFILES)
	$(LD) $(OFILES) $(LDLIBS) -o daisy-player

daisy-player.o: daisy-player.c
	$(CC) -Wall -Wunused -D PREFIX=\"$(PREFIX)\" -c $< -o $@

clean:
	rm -f *.o daisy-player

install:
	@./install.sh $(PREFIX)
