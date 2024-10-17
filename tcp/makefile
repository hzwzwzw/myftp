CC = gcc
CFLAGS = -Wall -g
TARGET = server

all: $(TARGET)

$(TARGET): server.o
	$(CC) $(CFLAGS) -o $(TARGET) server.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f $(TARGET) server.o