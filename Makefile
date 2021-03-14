PLATFORM=$(shell uname | tr '[A-Z]' '[a-z]')-$(shell uname -m)
TARGETDIR=.
TARGETS=$(TARGET)
TARGET=charlotte-logger
OBJS=charlotte-logger.o fileupload.o hash.o
STATICLIBS=/usr/local/lib/libcurl.a
LDFLAGS=-lz -lssl -lcrypto -lpthread
CFLAGS=-g -Wall
VER=0.0.5
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(STATICLIBS)

charlotte-logger.o:	charlotte-logger.c
			$(CC) $(CFLAGS) -c charlotte-logger.c

fileupload.o:	fileupload.c
			$(CC) $(CFLAGS) -c fileupload.c

hash.o:		hash.c
			$(CC) $(CFLAGS) -c hash.c

clean:
	-rm -f $(TARGETS) $(OBJS) *.elf *.gdb


