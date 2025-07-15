# CompactText
Convierte un texto en una representación compacta (IDs de palabras) y viceversa, con versiones paralelas en **OpenMP y MPI**.

_Proyecto 3 de Calcolo Paralello e Distribuito._

## Estructura del proyecto
```
CompactText
├── data/                   # textos de prueba
├── src/
│   ├── mpi/                # versión MPI
│   │   ├── mpi.c           # código principal MPI
│   │   └── makefile        # makefile para compilar la versión MPI
│   ├── openmp/             # versión OpenMP
│   │   ├── omp.c           # código principal OpenMP
│   │   └── makefile        # makefile para compilar la versión Open
│   └── sequential/         # versión secuencial
│       ├── seq.c           # código principal secuencial
│       └── makefile        # makefile para compilar la versión secuencial
├── .gitignore              
├── memory.docx             # Memoria del proyecto
└── README.md
```

## Autores
- Sara Liñan Heredia
- Antonio García Torres