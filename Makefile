CC      = gcc
CFLAGS  = -Wall -g -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security
PREFIX  = /usr/local/
LD      = $(CC)
LDLIBS  = -lncursesw -lidn -lsox -lmxml
OFILES  = daisy-player.o daisy3.o

all: $(OFILES)
	$(LD) $(OFILES) $(LDLIBS) -o daisy-player

daisy-player.o: daisy-player.c
	$(CC) $(CFLAGS) -D PREFIX=\"$(PREFIX)\" -c $< -o $@

daisy3.o: daisy3.c
	$(CC) $(CFLAGS) -D PREFIX=\"$(PREFIX)\" -c $< -o $@

clean:
	rm -f *.o daisy-player

install:
	@./install.sh $(PREFIX)
