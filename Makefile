CC      = gcc
CFLAGS  = -Wall -Wextra -Wunused -g -I /usr/include/libxml2/
PREFIX  = /usr/local/
LD      = $(CC)
LDLIBS  = -lncursesw -lsox -lxml2
OFILES  = daisy-player.o daisy3.o audiocd.o

all: $(OFILES)
	$(LD) $(OFILES) $(LDLIBS) -o daisy-player

daisy-player.o: daisy-player.c
	$(CC) $(CFLAGS) -D PREFIX=\"$(PREFIX)\" -c $< -o $@

daisy3.o: daisy3.c
	$(CC) $(CFLAGS) -D PREFIX=\"$(PREFIX)\" -c $< -o $@
	
audiocd.o: audiocd.c
	$(CC) $(CFLAGS) -D PREFIX=\"$(PREFIX)\" -c $< -o $@

clean:
	rm -f *.o daisy-player daisy-player.1 daisy-player.html

install:
	@./install.sh $(PREFIX)
