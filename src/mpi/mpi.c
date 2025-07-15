#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LEN 256
#define MAX_DICT_SIZE 100000

typedef struct {
    char word[MAX_WORD_LEN];
    int id;
} DictEntry;

DictEntry dict[MAX_DICT_SIZE];
int dict_size = 0;

int find_word(const char *word) {
    for (int i = 0; i < dict_size; i++) {
        if (strcmp(dict[i].word, word) == 0)
            return dict[i].id;
    }
    return -1;
}

int add_word(const char *word) {
    int id = dict_size;
    strcpy(dict[dict_size].word, word);
    dict[dict_size].id = id;
    dict_size++;
    return id;
}

void save_dict(const char *filename) {
    FILE *f = fopen(filename, "w");
    for (int i = 0; i < dict_size; i++) {
        fprintf(f, "%s\n", dict[i].word);
    }
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

void encode_file(const char *input_filename) {
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "encoded_%s.bin", input_filename);

    FILE *in = fopen(input_filename, "r");
    FILE *out = fopen(output_filename, "wb");
    if (!in || !out) {
        fprintf(stderr, "No se pudo abrir %s\n", input_filename);
        return;
    }

    char word[MAX_WORD_LEN];
    int c, idx = 0;

    while ((c = fgetc(in)) != EOF) {
        if (isalnum(c)) {
            word[idx++] = c;
        } else {
            if (idx > 0) {
                word[idx] = '\0';
                int id = find_word(word);
                if (id == -1) {
                    id = add_word(word);
                }
                fwrite(&id, sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;
            fwrite(&sep, sizeof(int), 1, out);
        }
    }

    if (idx > 0) {
        word[idx] = '\0';
        int id = find_word(word);
        if (id == -1) {
            id = add_word(word);
        }
        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);
}

void decode_file(const char *bin_filename) {
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "decoded_%s.txt", bin_filename + 8);

    FILE *in = fopen(bin_filename, "rb");
    FILE *out = fopen(output_filename, "w");
    if (!in || !out) {
        fprintf(stderr, "No se pudo abrir archivo binario: %s\n", bin_filename);
        return;
    }

    int token;
    while (fread(&token, sizeof(int), 1, in)) {
        if (token >= 0) {
            fprintf(out, "%s", dict[token].word);
        } else {
            fputc(-token, out);
        }
    }

    fclose(in);
    fclose(out);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0)
            printf("Uso: %s [encode|decode] archivo1 archivo2 ...\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    char *command = argv[1];
    int n_files = argc - 2;

    // El proceso 0 carga el diccionario antes de decode
    if (rank == 0 && strcmp(command, "decode") == 0) {
        load_dict("dict.txt");
    }

    // Broadcast tamaño dict y dict al resto
    MPI_Bcast(&dict_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    for (int i = 0; i < dict_size; i++) {
        MPI_Bcast(dict[i].word, MAX_WORD_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);
        MPI_Bcast(&dict[i].id, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }

    double start_time = MPI_Wtime();

    // Cada proceso maneja archivos distribuidos round robin
    for (int i = rank; i < n_files; i += size) {
        if (strcmp(command, "encode") == 0) {
            encode_file(argv[i + 2]);
        } else if (strcmp(command, "decode") == 0) {
            decode_file(argv[i + 2]);
        } else {
            if (rank == 0) printf("Comando no reconocido: %s\n", command);
            MPI_Finalize();
            return 1;
        }
    }

    // Ahora proceso 0 recoge los diccionarios de los demás para encode
    if (strcmp(command, "encode") == 0) {
        // Proceso 0 recibe dict_size y dict arrays de los demás y los combina (simple)
        if (rank != 0) {
            // Enviar dict_size
            MPI_Send(&dict_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            // Enviar palabras y ids
            for (int i = 0; i < dict_size; i++) {
                MPI_Send(dict[i].word, MAX_WORD_LEN, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
                MPI_Send(&dict[i].id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }
        } else {
            for (int src = 1; src < size; src++) {
                int recv_dict_size;
                MPI_Recv(&recv_dict_size, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                for (int j = 0; j < recv_dict_size; j++) {
                    char rword[MAX_WORD_LEN];
                    int rid;
                    MPI_Recv(rword, MAX_WORD_LEN, MPI_CHAR, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&rid, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    // Añadir palabra si no existe
                    if (find_word(rword) == -1) {
                        add_word(rword);
                    }
                }
            }
            save_dict("dict.txt");
        }
    }

    double end_time = MPI_Wtime();

    if (rank == 0) {
        printf("Tiempo total (MPI): %.4f segundos\n", end_time - start_time);
    }

    MPI_Finalize();
    return 0;
}

