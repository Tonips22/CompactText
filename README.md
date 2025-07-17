# CompactText
Converte un testo in una rappresentazione compatta (ID delle parole) e viceversa, con versioni parallele in **OpenMP e MPI**.

_Progetto 3 di Calcolo Parallelo e Distribuito._

## 1. Struttura del progetto
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
```

## 2. Compilazione ed Esecuzione
### Versione Sequenziale / OpenMP
**Accedere** alla cartella `src/sequential`
```bash
cd src/sequential ó cd src/openmp
```
**Copia** i testi di prova dalla directory `data` alla directory corrente, ad esempio:
```bash
cp ../../data/S/* .
```
**Compilare** il codice:
```bash
make
```
**Assegna** il numero di thread (OpenMP):
```bash
export OMP_NUM_THREADS=4  # o il numero desiderato di thread
```

**Esegui** il encoder:
```bash
make encode
```

**Esegui** il decoder:
```bash
make decode
```
**Pulisci** i file generati:
```bash
make clean
```

### Versión MPI
**Accedere** alla cartella `src/mpi`
```bash
cd src/mpi
```
**Copia** i testi di prova dalla directory `data` alla directory corrente, ad esempio:
```bash
cp ../../data/S/* .
```
**Compilare** il codice MPI:
```bash
make
```
**Esegui** il encoder (MPI):
```bash
make encode MPI_PROCS=4 # o il numero desiderato di thread
```
**Esegui** il decoder (MPI):
```bash
make decode MPI_PROCS=4 # o il numero desiderato di thread
```
**Pulisci** i file generati:
```bash
make clean
```


## 3. Tests y Memoria
- [CompactText Tests](https://docs.google.com/spreadsheets/d/1AYNYCpEJc7D6Up8Y7-oKM3t9jygzSl_Xlc6H8CmSi3I/edit?usp=sharing) dove abbiamo annotato tutti i test effettuati.
- [Memoria del proyecto](https://docs.google.com/document/d/1zgazvNnEc1EiDpZZGMbx4p7WmcHdK6b3cvMjiNtbz8w/edit?tab=t.0) con una descrizione dettagliata dell’implementazione, dei problemi affrontati e dei risultati ottenuti.

## 4. Autores
- Sara Liñan Heredia
- Antonio García Torres
