CC = gcc
LFAGS = 
CFLAGS = -Wall -O3
OBJECTS = screensweeper.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

screensweaper: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
debug:	CFLAGS= -Wall -O0 -g
debug:	$(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o screensweaper-$@ $(LDFLAGS)
clean:
	@echo cleaning oop
	@rm -f $(OBJECTS)
	@rm -f screensweaper
	@rm -f screensweaper-debug














