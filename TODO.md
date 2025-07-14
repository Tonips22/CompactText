# TODO
- Integración de paréntesis, llaves, corchetes, comillas y comillas simples... en el encoder. ✅
- Añadir al README ruta de instalación y uso.
- Añadir función para comparar tiempos de ejecución entre OpenMP, MPI y secuencial. ✅
- Cambiar lenguaje de C++ --> C.
- Archivos:
    - **Low text:** 100 <= words <= 150
    - **Medium text:** 1000 <= words <= 1500
    - **High text:** 10000 <= words <= 15000

- Comandos de pruebas:
    - **Secuencial:**
        - ./compacttext_seq encode ../data/high_text1.txt ../data/high_text2.txt ../data/high_text3.txt
        - ./compacttext_seq decode ../data/high_text1 ../data/high_text2 ../data/high_text3

        - ./compacttext_seq encode ../data/medium_text1.txt ../data/medium_text2.txt ../data/medium_text3.txt
        - ./compacttext_seq decode ../data/medium_text1 ../data/medium_text2 ../data/medium_text3

        - ./compacttext_seq encode ../data/low_text1.txt ../data/low_text2.txt ../data/low_text3.txt
        - ./compacttext_seq decode ../data/low_text1 ../data/low_text2 ../data/low_text3
    - **OpenMP:**
        - ./compacttext_omp encode ../data/high_text1.txt ../data/high_text2.txt ../data/high_text3.txt
        - ./compacttext_omp decode ../data/high_text1 ../data/high_text2 ../data/high_text3

        - ./compacttext_omp encode ../data/medium_text1.txt ../data/medium_text2.txt ../data/medium_text3.txt
        - ./compacttext_omp decode ../data/medium_text1 ../data/medium_text2 ../data/medium_text3

        - ./compacttext_omp encode ../data/low_text1.txt ../data/low_text2.txt ../data/low_text3.txt
        - ./compacttext_omp decode ../data/low_text1 ../data/low_text2 ../data/low_text3
    - **MPI:**
        - mpirun -np 4 ./compacttext_mpi encode ../data/high_text1.txt ../data/high_text2.txt ../data/high_text3.txt
        - mpirun -np 4 ./compacttext_mpi decode ../data/high_text1 ../data/high_text2 ../data/high_text3

        - mpirun -np 4 ./compacttext_mpi encode ../data/medium_text1.txt ../data/medium_text2.txt ../data/medium_text3.txt
        - mpirun -np 4 ./compacttext_mpi decode ../data/medium_text1 ../data/medium_text2 ../data/medium_text3

        - mpirun -np 4 ./compacttext_mpi encode ../data/low_text1.txt ../data/low_text2.txt ../data/low_text3.txt
        - mpirun -np 4 ./compacttext_mpi decode ../data/low_text1 ../data/low_text2 ../data/low_text3
    - **Eliminar binarios & recon:**
        - rm ../data/*.bin && rm ../data/*recon.txt
