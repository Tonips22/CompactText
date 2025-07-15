# TODO
- Integración de paréntesis, llaves, corchetes, comillas y comillas simples... en el encoder. ✅
- Añadir al README ruta de instalación y uso.
- Añadir función para comparar tiempos de ejecución entre OpenMP, MPI y secuencial. ✅
- Cambiar lenguaje de C++ --> C.
- Comentar y explicar en documentación de hpp
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


# Conclusiones hasta el momento:
- El uso de OpenMP mejora el rendimiento en comparación con la versión secuencial cuando se trata de codificar y decodificar varios archivos de texto grandes a la vez.
- Cuando es un único archivo, el rendimiento de OpenMP no es significativamente mejor que el secuencial, y en algunos casos puede ser incluso más lento debido a la sobrecarga de la paralelización.
- La implementación de OpenMP es más eficiente cuando se trabaja con múltiples archivos, ya que permite distribuir la carga de trabajo entre varios hilos, lo que reduce el tiempo total de procesamiento.
- La versión secuencial es más adecuada para tareas simples o cuando se trabaja con un solo archivo pequeño o grande.
