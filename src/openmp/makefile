CC = gcc
CFLAGS = -O2 -Wall -fopenmp
TARGET = omp

all: $(TARGET)

$(TARGET): omp.c
	$(CC) $(CFLAGS) -o $(TARGET) omp.c

encode:
	./$(TARGET) encode *.txt

decode:
	./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

