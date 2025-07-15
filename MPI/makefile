CC = mpicc
CFLAGS = -O2 -Wall
TARGET = main_mpi

all: $(TARGET)

$(TARGET): main_mpi.c
	$(CC) $(CFLAGS) -o $(TARGET) main_mpi.c

encode:
	mpirun -np 4 ./$(TARGET) encode input1.txt input2.txt

decode:
	mpirun -np 4 ./$(TARGET) decode encoded_input1.txt.bin encoded_input2.txt.bin

clean:
	rm -f $(TARGET) *.o encoded_*.bin decoded_*.txt dict.txt

