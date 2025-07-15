CC = gcc
CFLAGS = -O2 -Wall -fopenmp
TARGET = main_openmp
SRC = main_openmp.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

encode: $(TARGET)
	./$(TARGET) encode input1.txt input2.txt

decode: $(TARGET)
	./$(TARGET) decode encoded_input1.txt.bin encoded_input2.txt.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt


