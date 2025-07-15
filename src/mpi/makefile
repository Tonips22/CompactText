CC = mpicc
CFLAGS = -O2 -Wall
TARGET = mpi

all: $(TARGET)

$(TARGET): mpi.c
	$(CC) $(CFLAGS) -o $(TARGET) mpi.c

encode:
	mpirun -np 4 ./$(TARGET) encode *.txt

decode:
	mpirun -np 4 ./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

