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
    if (!f) {
        perror("No se pudo guardar el diccionario");
        exit(1);
    }
    for (int i = 0; i < dict_size; i++) {
        fprintf(f, "%s\n", dict[i].word);
    }
    fclose(f);
}

void load_dict(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("No se pudo cargar el diccionario");
        exit(1);
    }
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
    snprintf(output_filename, sizeof(output_filename), "decoded_%s.txt", bin_filename + 8); // asumes "encoded_"

    FILE *in = fopen(bin_filename, "rb");
    if (!in) {
        perror("No se pudo abrir archivo binario");
        return;
    }

    FILE *out = fopen(output_filename, "w");
    if (!out) {
        perror("No se pudo crear archivo de salida de texto");
        fclose(in);
        return;
    }

    int token;
    while (fread(&token, sizeof(int), 1, in)) {
        if (token >= 0) {
            if (token >= dict_size) {
                fprintf(stderr, "Error: token %d fuera de rango (dict_size=%d)\n", token, dict_size);
                fclose(in);
                fclose(out);
                exit(1);
            }
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

    clock_t start = clock();

    if (strcmp(argv[1], "encode") == 0) {
        // Secuencial: procesa cada archivo uno por uno
        for (int i = 2; i < argc; i++) {
            encode_file(argv[i]);
        }
        save_dict("dict.txt");
    } else if (strcmp(argv[1], "decode") == 0) {
        load_dict("dict.txt");
        for (int i = 2; i < argc; i++) {
            decode_file(argv[i]);
        }
    } else {
        printf("Comando no reconocido: %s\n", argv[1]);
        return 1;
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tiempo total (Secuencial): %.4f segundos\n", time_spent);

    return 0;
}

