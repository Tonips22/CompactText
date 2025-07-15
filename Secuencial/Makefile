CC = gcc
CFLAGS = -O2 -Wall

all: main

main_secuencial: main
	$(CC) $(CFLAGS) -o main_secuencial main_secuencial.c

encode:
	./main encode input1.txt input2.txt

decode:
	./main decode encoded_input1.txt.bin encoded_input2.txt.bin

clean:
	rm -f main encoded_*.bin decoded_*.txt dict.txt

