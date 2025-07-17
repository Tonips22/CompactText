# CompactText
Convierte un texto en una representación compacta (IDs de palabras) y viceversa, con versiones paralelas en **OpenMP y MPI**.

_Proyecto 3 de Calcolo Paralello e Distribuito._

## 1. Estructura del proyecto
```
CompactText
├── data/                   # testi di prova
├── src/
│   ├── mpi/                # versione MPI
│   │   ├── mpi.c           # codice principale MPI
│   │   └── makefile        # makefile per compilare la versione MPI
│   ├── openmp/             # versione OpenMP
│   │   ├── omp.c           # codice principale OpenMP
│   │   └── makefile        # makefile per compilare la versione OpenMP
│   └── sequential/         # versione sequenziale
│       ├── seq.c           # codice principale sequenziale
│       └── makefile        # makefile per compilare la versione sequenziale
├── .gitignore              
├── memory.docx             # Documento del progetto
└── README.md


## 2. Compilación y Ejecución
### Versión Secuencial / OpenMP
**Acceder** al directorio `src/sequential`
```bash
cd src/sequential ó cd src/openmp
```
**Copiar** los textos de prueba del directorio `data` al directorio actual, por ejemplo:
```bash
cp ../../data/S/* .
```
**Compilar** el código secuencial:
```bash
make
```
**Asignar** el número de threads (OpenMP):
```bash
export OMP_NUM_THREADS=4  # o el número de hilos deseado
```

**Ejecutar** el encoder:
```bash
make encode
```

**Ejecutar** el decoder:
```bash
make decode
```
**Limpiar** los archivos generados:
```bash
make clean
```

### Versión MPI
**Acceder** al directorio `src/mpi`
```bash
cd src/mpi
```
**Copiar** los textos de prueba del directorio `data` al directorio actual, por ejemplo:
```bash
cp ../../data/S/* .
```
**Compilar** el código MPI:
```bash
make
```
**Ejecutar** el encoder (MPI):
```bash
make encode MPI_PROCS=4 # o el número de procesos deseado
```
**Ejecutar** el decoder (MPI):
```bash
make decode MPI_PROCS=4 # o el número de procesos deseado
```
**Limpiar** los archivos generados:
```bash
make clean
```


## 3. Tests y Memoria
- [CompactText Tests](https://docs.google.com/spreadsheets/d/1AYNYCpEJc7D6Up8Y7-oKM3t9jygzSl_Xlc6H8CmSi3I/edit?usp=sharing) donde hemos anotado todos los tests realizados.
- [Memoria del proyecto](https://docs.google.com/document/d/1zgazvNnEc1EiDpZZGMbx4p7WmcHdK6b3cvMjiNtbz8w/edit?tab=t.0) con una descripción detallada de la implementación, problemas y resultados obtenidos.

## 4. Autores
- Sara Liñan Heredia
- Antonio García Torres
