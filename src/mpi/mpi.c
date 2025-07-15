/*****************************************************************
 CompactText — versión MPI con tiempos por archivo y nº de procesos
*****************************************************************/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_WORD_LEN 256
#define MAX_DICT_SIZE 100000

typedef struct {
    char word[MAX_WORD_LEN];
    int id;
} DictEntry;

static DictEntry dict[MAX_DICT_SIZE];
static int dict_size = 0;

// Busca palabra en dict, devuelve id o -1
int find_word(const char *word) {
    for (int i = 0; i < dict_size; i++)
        if (strcmp(dict[i].word, word) == 0)
            return dict[i].id;
    return -1;
}

// Añade palabra y devuelve nuevo id
int add_word(const char *word) {
    int id = dict_size;
    strcpy(dict[dict_size].word, word);
    dict[dict_size].id = id;
    dict_size++;
    return id;
}

void save_dict(const char *filename) {
    FILE *f = fopen(filename, "w");
    for (int i = 0; i < dict_size; i++)
        fprintf(f, "%s\n", dict[i].word);
    fclose(f);
}

void load_dict(const char *filename) {
    FILE *f = fopen(filename, "r");
    char word[MAX_WORD_LEN];
    dict_size = 0;
    while (fgets(word, MAX_WORD_LEN, f)) {
        word[strcspn(word, "\n")] = '\0';
        add_word(word);
    }
    fclose(f);
}

// Codifica un archivo, mide tiempo individual y lo imprime
void encode_file(const char *input_filename, int rank, int nprocs) {
    double t0 = MPI_Wtime();

    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "encoded_%s.bin", input_filename);

    FILE *in  = fopen(input_filename, "r");
    FILE *out = fopen(output_filename, "wb");
    if (!in || !out) {
        fprintf(stderr, "[Rank %d] Error abriendo %s\n", rank, input_filename);
        return;
    }

    char word[MAX_WORD_LEN];
    int  c, idx = 0;
    while ((c = fgetc(in)) != EOF) {
        if (isalnum(c)) {
            word[idx++] = c;
        } else {
            if (idx > 0) {
                word[idx] = '\0';
                int id = find_word(word);
                if (id < 0) id = add_word(word);
                fwrite(&id,   sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;
            fwrite(&sep, sizeof(int), 1, out);
        }
    }
    if (idx > 0) {
        word[idx] = '\0';
        int id = find_word(word);
        if (id < 0) id = add_word(word);
        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);

    double t1 = MPI_Wtime();
    printf("[Encode] Rank %d/%d  '%s' → '%s'  in %.4f s\n",
           rank, nprocs, input_filename, output_filename, t1 - t0);
}

// Decodifica un archivo, mide tiempo individual y lo imprime
void decode_file(const char *bin_filename, int rank, int nprocs) {
    double t0 = MPI_Wtime();

    const char *base = bin_filename + strlen("encoded_");
    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "decoded_%s.txt", base);

    FILE *in  = fopen(bin_filename, "rb");
    FILE *out = fopen(output_filename, "w");
    if (!in || !out) {
        fprintf(stderr, "[Rank %d] Error abriendo %s\n", rank, bin_filename);
        return;
    }

    int token;
    while (fread(&token, sizeof(int), 1, in) == 1) {
        if (token >= 0) {
            fprintf(out, "%s", dict[token].word);
        } else {
            fputc(-token, out);
        }
    }

    fclose(in);
    fclose(out);

    double t1 = MPI_Wtime();
    printf("[Decode] Rank %d/%d  '%s' → '%s'  in %.4f s\n",
           rank, nprocs, bin_filename, output_filename, t1 - t0);
}

void help(const char *prog) {
    fprintf(stderr, "Uso: %s [encode|decode] archivo1 archivo2 ...\n", prog);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (rank == 0)
        printf("\nMPI: using %d processes\n", nprocs);

    if (argc < 3) {
        if (rank == 0) help(argv[0]);
        MPI_Finalize();
        return 1;
    }
    const char *command = argv[1];
    int n_files = argc - 2;

    // Pre-carga del diccionario si decodificamos
    if (rank == 0 && strcmp(command, "decode") == 0) {
        load_dict("dict.txt");
    }
    // Broadcast dict
    MPI_Bcast(&dict_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int i = 0; i < dict_size; i++) {
        MPI_Bcast(dict[i].word, MAX_WORD_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);
        MPI_Bcast(&dict[i].id, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }

    double t_start = MPI_Wtime();

    // Round-robin: cada proceso maneja su subconjunto
    for (int i = rank; i < n_files; i += nprocs) {
        const char *fname = argv[i + 2];
        if (strcmp(command, "encode") == 0) {
            encode_file(fname, rank, nprocs);
        } else if (strcmp(command, "decode") == 0) {
            decode_file(fname, rank, nprocs);
        } else {
            if (rank == 0) fprintf(stderr, "Comando no reconocido: %s\n", command);
            MPI_Finalize();
            return 1;
        }
    }

    // Tras encode, recolectar y guardar dict en el proceso 0
    if (strcmp(command, "encode") == 0) {
        if (rank != 0) {
            MPI_Send(&dict_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            for (int i = 0; i < dict_size; i++) {
                MPI_Send(dict[i].word, MAX_WORD_LEN, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
                MPI_Send(&dict[i].id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }
        } else {
            for (int src = 1; src < nprocs; src++) {
                int rsize;
                MPI_Recv(&rsize, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                for (int j = 0; j < rsize; j++) {
                    char w[MAX_WORD_LEN];
                    int  id;
                    MPI_Recv(w, MAX_WORD_LEN, MPI_CHAR, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&id, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    if (find_word(w) < 0) add_word(w);
                }
            }
            save_dict("dict.txt");
        }
    }

    double t_end = MPI_Wtime();
    if (rank == 0)
        printf("Total wall time (MPI): %.4f s\n", t_end - t_start);

    MPI_Finalize();
    return 0;
}
