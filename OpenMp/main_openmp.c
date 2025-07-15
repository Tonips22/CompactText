#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <ctype.h>
#include <time.h>

#define MAX_WORD_LEN 256
#define MAX_DICT_SIZE 100000

typedef struct {
    char word[MAX_WORD_LEN];
    int id;
} DictEntry;

typedef struct {
    DictEntry entries[MAX_DICT_SIZE];
    int size;
} Dictionary;

Dictionary global_dict;
omp_lock_t dict_lock;
int global_dict_size;

int find_word(Dictionary *dict, const char *word) {
    for (int i = 0; i < dict->size; i++) {
        if (strcmp(dict->entries[i].word, word) == 0)
            return dict->entries[i].id;
    }
    return -1;
}

int add_word(Dictionary *dict, const char *word) {
    int id = dict->size;
    strcpy(dict->entries[dict->size].word, word);
    dict->entries[dict->size].id = id;
    dict->size++;
    return id;
}

void save_dict(Dictionary *dict, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error al guardar diccionario");
        return;
    }
    for (int i = 0; i < dict->size; i++) {
        fprintf(f, "%s\n", dict->entries[i].word);
    }
    fclose(f);
}

void load_dict(Dictionary *dict, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("No se pudo abrir dict.txt");
        exit(1);
    }
    char word[MAX_WORD_LEN];
    dict->size = 0;
    while (fgets(word, MAX_WORD_LEN, f)) {
        word[strcspn(word, "\n")] = '\0';
        add_word(dict, word);
    }
    fclose(f);
}

void encode_file(const char *input_filename) {
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "encoded_%s.bin", input_filename);

    FILE *in = fopen(input_filename, "r");
    if (!in) {
        perror("No se pudo abrir archivo de entrada");
        return;
    }

    FILE *out = fopen(output_filename, "wb");
    if (!out) {
        perror("No se pudo crear archivo de salida");
        fclose(in);
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
                int id;

                omp_set_lock(&dict_lock);
                id = find_word(&global_dict, word);
                if (id == -1) {
                    id = add_word(&global_dict, word);
                }
                omp_unset_lock(&dict_lock);

                fwrite(&id, sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;
            fwrite(&sep, sizeof(int), 1, out);
        }
    }

    if (idx > 0) {
        word[idx] = '\0';
        int id;

        omp_set_lock(&dict_lock);
        id = find_word(&global_dict, word);
        if (id == -1) {
            id = add_word(&global_dict, word);
        }
        omp_unset_lock(&dict_lock);

        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);
}

void decode_file(const char *bin_filename) {
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "decoded_%s.txt", bin_filename + 8); // remove "encoded_"

    FILE *in = fopen(bin_filename, "rb");
    if (!in) {
        perror("No se pudo abrir archivo binario");
        return;
    }

    FILE *out = fopen(output_filename, "w");
    if (!out) {
        perror("No se pudo crear archivo de salida");
        fclose(in);
        return;
    }

    int token;
    while (fread(&token, sizeof(int), 1, in)) {
        if (token >= 0) {
            if (token < global_dict_size)
                fprintf(out, "%s", global_dict.entries[token].word);
            else
                fprintf(out, "<?>"); // error de índice
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

    double start_time = omp_get_wtime();

    omp_init_lock(&dict_lock);

    if (strcmp(argv[1], "encode") == 0) {
        global_dict.size = 0;

        #pragma omp parallel for schedule(dynamic)
        for (int i = 2; i < argc; i++) {
            encode_file(argv[i]);
        }

        // Guardar diccionario una vez
        save_dict(&global_dict, "dict.txt");

    } else if (strcmp(argv[1], "decode") == 0) {
        load_dict(&global_dict, "dict.txt");
        global_dict_size = global_dict.size;

        #pragma omp parallel for schedule(dynamic)
        for (int i = 2; i < argc; i++) {
            decode_file(argv[i]);
        }

    } else {
        printf("Comando no reconocido: %s\n", argv[1]);
        return 1;
    }

    omp_destroy_lock(&dict_lock);

    double end_time = omp_get_wtime();
    printf("Tiempo total (OpenMP): %.4f segundos\n", end_time - start_time);
    return 0;
}


