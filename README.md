# CompactText - Calcolo Parallelo e Distribuito
**Hecho por:**
- Sara Liñan Heredia
- Antonio García Torres

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