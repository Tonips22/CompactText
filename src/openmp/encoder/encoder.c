// encoder_openmp.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <ctype.h>

#define MAX_WORD_LEN 64
#define MAX_WORDS 100000

typedef struct {
    char word[MAX_WORD_LEN];
    int id;
} DictEntry;

DictEntry dictionary[MAX_WORDS];
int dict_size = 0;
omp_lock_t dict_lock;

int find_or_add_word(const char* word) {
    // Buscar palabra en el diccionario
    for (int i = 0; i < dict_size; ++i) {
        if (strcmp(dictionary[i].word, word) == 0) {
            return dictionary[i].id;
        }
    }
    // No encontrada → añadir
    strcpy(dictionary[dict_size].word, word);
    dictionary[dict_size].id = dict_size + 1;
    return dictionary[dict_size++].id;
}

void tokenize_and_encode(const char* filename, const char* output_filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("Error abriendo el archivo");
        exit(1);
    }

    FILE* out = fopen(output_filename, "wb");
    if (!out) {
        perror("Error creando archivo binario");
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char* token = strtok(line, " \t\n\r.,;:!?\"()[]{}");
        while (token != NULL) {
            char word[MAX_WORD_LEN];
            strncpy(word, token, MAX_WORD_LEN);
            word[MAX_WORD_LEN - 1] = '\0';

            // Convertir a minúsculas
            for (int i = 0; word[i]; i++) {
                word[i] = tolower(word[i]);
            }

            int id;
            omp_set_lock(&dict_lock);
            id = find_or_add_word(word);
            omp_unset_lock(&dict_lock);

            fwrite(&id, sizeof(int), 1, out);
            token = strtok(NULL, " \t\n\r.,;:!?\"()[]{}");
        }
    }

    fclose(f);
    fclose(out);
}

void save_dictionary(const char* filename) {
    FILE* f = fopen(filename, "w");
    for (int i = 0; i < dict_size; ++i) {
        fprintf(f, "%s %d\n", dictionary[i].word, dictionary[i].id);
    }
    fclose(f);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Uso: %s archivo1.txt [archivo2.txt ...]\n", argv[0]);
        return 1;
    }

    omp_init_lock(&dict_lock);

    #pragma omp parallel for
    for (int i = 1; i < argc; ++i) {
        char outname[256];
        snprintf(outname, sizeof(outname), "data/output%d.bin", i);
        tokenize_and_encode(argv[i], outname);
    }

    save_dictionary("data/vocab.dict");
    omp_destroy_lock(&dict_lock);

    return 0;
}
