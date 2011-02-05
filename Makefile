CC      = gcc
CFLAGS  = -Wall -Wunused
LD      = $(CC)
LDLIBS  = -lncursesw -lcdio # -lmpeg3 -lasound

OFILES = daisy-player.o # playmp3.o

all: $(OFILES)
	$(LD) $(CFLAGS) $(OFILES) $(LDLIBS) -o daisy-player -s

daisy-player.o: daisy-player.c
	$(CC) $(CFLAGS) -c $< -o $@

playmp3.o: playmp3.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o daisy-player

install:
	@./install.sh
