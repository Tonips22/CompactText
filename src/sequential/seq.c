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
    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "encoded_%s.bin", input_filename);

    clock_t t0 = clock();

    FILE *in = fopen(input_filename, "r");
    FILE *out = fopen(output_filename, "wb");
    if (!in || !out) {
        fprintf(stderr, "[Encode] Error abriendo '%s'\n", input_filename);
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
                if (id == -1) id = add_word(word);
                fwrite(&id, sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;  // codifica separadores como negativos
            fwrite(&sep, sizeof(int), 1, out);
        }
    }
    if (idx > 0) {
        word[idx] = '\0';
        int id = find_word(word);
        if (id == -1) id = add_word(word);
        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);

    clock_t t1 = clock();
    double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("[Encode] '%s' → '%s'  in %.4f s\n",
           input_filename, output_filename, elapsed);
}

void decode_file(const char *bin_filename) {
    const char *base = bin_filename + strlen("encoded_");
    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "decoded_%s.txt", base);

    clock_t t0 = clock();

    FILE *in = fopen(bin_filename, "rb");
    FILE *out = fopen(output_filename, "w");
    if (!in || !out) {
        fprintf(stderr, "[Decode] Error abriendo '%s'\n", bin_filename);
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

    clock_t t1 = clock();
    double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("[Decode] '%s' → '%s'  in %.4f s\n",
           bin_filename, output_filename, elapsed);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s [encode|decode] archivo1 archivo2 ...\n", argv[0]);
        return 1;
    }

    clock_t total_start = clock();

    if (strcmp(argv[1], "encode") == 0) {
        for (int i = 2; i < argc; i++) {
            encode_file(argv[i]);
        }
        save_dict("dict.txt");
    }
    else if (strcmp(argv[1], "decode") == 0) {
        load_dict("dict.txt");
        for (int i = 2; i < argc; i++) {
            decode_file(argv[i]);
        }
    }
    else {
        printf("Comando no reconocido: %s\n", argv[1]);
        return 1;
    }

    clock_t total_end = clock();
    double total_elapsed = (double)(total_end - total_start) / CLOCKS_PER_SEC;
    printf("Tiempo total (Secuencial): %.4f s\n", total_elapsed);
    return 0;
}
