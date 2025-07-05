# CompactText
Convierte un texto en una representación compacta (IDs de palabras) y viceversa, con versiones paralelas en **OpenMP y MPI**.

_Proyecto 3 de Calcolo Paralello e Distribuito._

## Estructura del proyecto
```
CompactText
├── README.md
├── memory.docx            # Memoria del proyecto
├── data/                  # textos de prueba
├── build/                 # binarios generados
├── src/
│   ├── common.hpp
│   ├── sequential.cpp     # referencia secuencial pequeña
│   ├── omp.cpp            # versión OpenMP
│   └── mpi.cpp            # versión MPI
└── CMakeLists.txt
```

## Autores
- Sara Liñan Heredia
- Antonio García Torres