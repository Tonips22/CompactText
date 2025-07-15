CC = mpicc
CFLAGS = -O2 -Wall
TARGET = mpi
MPI_PROCS ?= 4

$(TARGET): mpi.c
	$(CC) $(CFLAGS) -o $(TARGET) mpi.c

encode:
	mpirun -np $(MPI_PROCS) ./$(TARGET) encode *.txt

decode:
	mpirun -np $(MPI_PROCS) ./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

