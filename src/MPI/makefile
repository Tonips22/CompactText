CC = mpicc
CFLAGS = -O2 -Wall
TARGET = main_mpi

all: $(TARGET)

$(TARGET): main_mpi.c
	$(CC) $(CFLAGS) -o $(TARGET) main_mpi.c

encode:
	mpirun -np 4 ./$(TARGET) encode *.txt

decode:
	mpirun -np 4 ./$(TARGET) decode encoded_*.bin

clean:
	rm -f $(TARGET) encoded_*.bin decoded_*.txt dict.txt

