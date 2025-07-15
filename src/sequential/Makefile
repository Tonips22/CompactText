CC = gcc
CFLAGS = -O2 -Wall
TARGET = seq

$(TARGET): seq.c
	$(CC) $(CFLAGS) -o $(TARGET) seq.c

encode:
	./$(TARGET) encode *.txt

decode:
	./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

