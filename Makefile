CC = gcc
OBJ = server.o
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
	rm -f *.o
	rm -f $(TARGET)
