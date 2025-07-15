CC = gcc
CFLAGS = -O2 -Wall -fopenmp
TARGET = main_openmp

all: $(TARGET)

$(TARGET): main_openmp.c
	$(CC) $(CFLAGS) -o $(TARGET) main_openmp.c

encode:
	./$(TARGET) encode *.txt

decode:
	./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

