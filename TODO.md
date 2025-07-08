# TODO
- Integración de paréntesis, llaves, corchetes, comillas y comillas simples... en el encoder. ✅
- Añadir al README ruta de instalación y uso.
- Añadir función para comparar tiempos de ejecución entre OpenMP, MPI y secuencial.


mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

export OMP_NUM_THREADS=4
./compacttext_omp encode ../data/messi1.txt ../data/messi2.txt


./compacttext_omp decode ../data/messi1.bin ../data/messi2.bin


diff -q ../data/messi1.txt ../data/messi1_decoded.txt
diff -q ../data/messi2.txt ../data/messi2_decoded.txt


en mpi se hace asi:
mpirun -np 4 ./compacttext_mpi encode ../data/messi1.txt ../data/messi2.txt
mpirun -np 4 ./compacttext_mpi decode messi1.bin messi2.bin



