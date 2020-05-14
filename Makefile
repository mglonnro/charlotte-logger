PLATFORM=$(shell uname | tr '[A-Z]' '[a-z]')-$(shell uname -m)
TARGETDIR=.
TARGETS=$(TARGET)
TARGET=charlotte-logger
OBJS=charlotte-logger.o fileupload.o hash.o
LDFLAGS=-lcurl -lcrypto
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS$(LDLIBS-$(@)))

charlotte-logger.o:	charlotte-logger.c
			$(CC) $(CFLAGS) -c charlotte-logger.c

fileupload.o:	fileupload.c
			$(CC) $(CFLAGS) -c fileupload.c

hash.o:		hash.c
			$(CC) $(CFLAGS) -c hash.c

clean:
	-rm -f $(TARGETS) $(OBJS) *.elf *.gdb


