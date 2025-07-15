CC = gcc
CFLAGS = -O2 -Wall
TARGET = main

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

encode:
	./$(TARGET) encode *.txt

decode:
	./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

