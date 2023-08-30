CC 		= g++
CFLAGS  = -Wall -Wextra -O2 -pthread 
.PHONY: all clean

all: sikradio-sender sikradio-receiver 

sikradio-sender sikradio-receiver:
	$(CC) $(CFLAGS) $@.cpp err.h utils.h -o $@
clean:
	rm -rf *.o sikradio-receiver sikradio-sender
