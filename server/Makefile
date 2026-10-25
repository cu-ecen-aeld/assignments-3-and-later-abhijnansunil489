CC     = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -g
TARGET = aesdsocket
SRC    = aesdsocket.c

.PHONY : clean all default

all default : $(TARGET) 

$(TARGET) : aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@

clean :
	rm -f $(TARGET)
