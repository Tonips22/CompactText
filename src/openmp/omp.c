#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <omp.h>
#include <time.h>

#define MAX_WORD_LEN 256
#define MAX_DICT_SIZE 100000

typedef struct {
    char word[MAX_WORD_LEN];
    int id;
} DictEntry;

static DictEntry dict[MAX_DICT_SIZE];
static int dict_size = 0;
static omp_lock_t dict_lock;

// Busca una palabra en el diccionario, devuelve -1 si no existe
int find_word(const char *word) {
    for (int i = 0; i < dict_size; i++) {
        if (strcmp(dict[i].word, word) == 0)
            return dict[i].id;
    }
    return -1;
}

// Añade una palabra nueva al diccionario y devuelve su id
int add_word(const char *word) {
    int id = dict_size;
    strcpy(dict[dict_size].word, word);
    dict[dict_size].id = id;
    dict_size++;
    return id;
}

// Guarda el diccionario en un fichero de texto (una palabra por línea)
void save_dict(const char *filename) {
    FILE *f = fopen(filename, "w");
    for (int i = 0; i < dict_size; i++) {
        fprintf(f, "%s\n", dict[i].word);
    }
    fclose(f);
}

// Carga el diccionario desde un fichero de texto
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

// Codifica un único archivo
void encode_file(const char *input_filename) {
    double t0 = omp_get_wtime();

    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "encoded_%s.bin", input_filename);

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
                int id;
                omp_set_lock(&dict_lock);
                id = find_word(word);
                if (id == -1)
                    id = add_word(word);
                omp_unset_lock(&dict_lock);

                fwrite(&id, sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;  // codificamos el separador como negativo
            fwrite(&sep, sizeof(int), 1, out);
        }
    }
    // última palabra si no acabó en separador
    if (idx > 0) {
        word[idx] = '\0';
        int id;
        omp_set_lock(&dict_lock);
        id = find_word(word);
        if (id == -1)
            id = add_word(word);
        omp_unset_lock(&dict_lock);
        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);

    double t1 = omp_get_wtime();
    int tid = omp_get_thread_num();
    int nthreads = omp_get_num_threads();
    #pragma omp critical
    printf("[Encode] Thread %d/%d  '%s'  ->  '%s'  in  %.4f s\n",
           tid, nthreads, input_filename, output_filename, t1 - t0);
}

// Decodifica un único archivo binario
void decode_file(const char *bin_filename) {
    double t0 = omp_get_wtime();

    // construimos el nombre out: quitamos el "encoded_" y ponemos "decoded_"
    const char *base = bin_filename + strlen("encoded_");
    char output_filename[512];
    snprintf(output_filename, sizeof(output_filename),
             "decoded_%s.txt", base);

    FILE *in = fopen(bin_filename, "rb");
    FILE *out = fopen(output_filename, "w");
    if (!in || !out) {
        fprintf(stderr, "No se pudo abrir binario: %s\n", bin_filename);
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

    double t1 = omp_get_wtime();
    int tid = omp_get_thread_num();
    int nthreads = omp_get_num_threads();
    #pragma omp critical
    printf("[Decode] Thread %d/%d  '%s'  ->  '%s'  in  %.4f s\n",
           tid, nthreads, bin_filename, output_filename, t1 - t0);
}

void help(const char *prog) {
    printf("Usage: %s [-t num_threads] [encode|decode] file1 file2 ...\n", prog);
    printf("  -t N   Número de threads OpenMP (por defecto: OMP_NUM_THREADS o hardware)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        help(argv[0]);
        return 1;
    }

    // 1) Leer -t N opcional
    int argi = 1;
    int num_threads = omp_get_max_threads();
    if (strcmp(argv[argi], "-t") == 0 && argi+1 < argc) {
        num_threads = atoi(argv[argi+1]);
        if (num_threads > 0) omp_set_num_threads(num_threads);
        argi += 2;
    }

    printf("\nOpenMP: using %d threads\n", num_threads);

    omp_init_lock(&dict_lock);
    double t_start = omp_get_wtime();

    // 2) Encode o decode en paralelo todos los ficheros
    if (strcmp(argv[argi], "encode") == 0) {
        #pragma omp parallel for schedule(dynamic)
        for (int i = argi+1; i < argc; i++) {
            encode_file(argv[i]);
        }
        #pragma omp single
        save_dict("dict.txt");

    } else if (strcmp(argv[argi], "decode") == 0) {
        load_dict("dict.txt");
        #pragma omp parallel for schedule(dynamic)
        for (int i = argi+1; i < argc; i++) {
            decode_file(argv[i]);
        }

    } else {
        help(argv[0]);
        omp_destroy_lock(&dict_lock);
        return 1;
    }

    double t_end = omp_get_wtime();
    omp_destroy_lock(&dict_lock);

    printf("Total wall time (OpenMP): %.4f s\n", t_end - t_start);
    return 0;
}