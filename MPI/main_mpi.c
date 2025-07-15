#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mpi.h>

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
    if (argc < 3) {
        printf("Uso: %s [encode|decode] archivo1 archivo2 ...\n", argv[0]);
        return 1;
    }

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double start = MPI_Wtime();


    if (strcmp(argv[1], "encode") == 0) {
        for (int i = 2 + rank; i < argc; i += size) {
            encode_file(argv[i]);
        }

        // Unir diccionarios (solo root guarda uno final)
        if (rank == 0) {
            save_dict("dict.txt");
        }

    } else if (strcmp(argv[1], "decode") == 0) {
        load_dict("dict.txt");

        for (int i = 2 + rank; i < argc; i += size) {
            decode_file(argv[i]);
        }

    } else {
        printf("Comando no reconocido: %s\n", argv[1]);
        MPI_Finalize();
        return 1;
    }

    double end = MPI_Wtime();
    if (rank == 0)
        printf("Tiempo total (MPI): %.4f segundos\n", end - start);

    MPI_Finalize();
    return 0;
}
