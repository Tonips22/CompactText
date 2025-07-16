/*****************************************************************
 CompactText — OpenMP corregido  
  • Diccionarios locales por hilo (cero locks)  
  • Fusión en hilo 0  
  • Tiempos individuales + total  
*****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <omp.h>

#define MAX_WORD_LEN 256
#define MAX_DICT_SIZE 100000

typedef struct { char word[MAX_WORD_LEN]; int id; } DictEntry;

// Diccionario global resultante
static DictEntry global_dict[MAX_DICT_SIZE];
static int       global_size = 0;

// Busca en un dict local o global
static int dict_find(const DictEntry *d, int size, const char *w) {
    for (int i = 0; i < size; i++)
        if (strcmp(d[i].word, w) == 0) return d[i].id;
    return -1;
}
// Añade a un dict local o global
static int dict_add(DictEntry *d, int *psize, const char *w) {
    int id = (*psize);
    strcpy(d[id].word, w);
    d[id].id = id;
    (*psize)++;
    return id;
}

// Guarda el diccionario global en dict.txt
static void save_dict(const char *filename) {
    FILE *f = fopen(filename, "w");
    for (int i = 0; i < global_size; i++)
        fprintf(f, "%s\n", global_dict[i].word);
    fclose(f);
}

// Carga dict.txt en global_dict
static void load_dict(const char *filename) {
    FILE *f = fopen(filename, "r");
    char w[MAX_WORD_LEN];
    global_size = 0;
    while (fgets(w, MAX_WORD_LEN, f)) {
        w[strcspn(w, "\n")] = 0;
        dict_add(global_dict, &global_size, w);
    }
    fclose(f);
}

// Codifica un archivo usando su dict local (sin locks)
static void encode_file(const char *fname, DictEntry *local, int *lsize) {
    double t0 = omp_get_wtime();
    char outname[512];
    snprintf(outname, sizeof(outname), "encoded_%s.bin", fname);

    FILE *in  = fopen(fname, "r");
    FILE *out = fopen(outname, "wb");
    if (!in || !out) { fprintf(stderr, "Err %s\n", fname); return; }

    char word[MAX_WORD_LEN];
    int c, idx = 0;
    while ((c = fgetc(in)) != EOF) {
        if (isalnum(c)) {
            word[idx++] = c;
        } else {
            if (idx) {
                word[idx] = 0;
                int id = dict_find(local, *lsize, word);
                if (id < 0) id = dict_add(local, lsize, word);
                fwrite(&id, sizeof(int), 1, out);
                idx = 0;
            }
            int sep = -c;
            fwrite(&sep, sizeof(int), 1, out);
        }
    }
    if (idx) {
        word[idx] = 0;
        int id = dict_find(local, *lsize, word);
        if (id < 0) id = dict_add(local, lsize, word);
        fwrite(&id, sizeof(int), 1, out);
    }

    fclose(in);
    fclose(out);

    printf("[Encode] T%2d %-20s in %.4f s\n",
           omp_get_thread_num(), fname, omp_get_wtime() - t0);
}

// Decodifica usando el global_dict
static void decode_file(const char *bin) {
    double t0 = omp_get_wtime();
    const char *base = bin + strlen("encoded_");
    char outname[512];
    snprintf(outname, sizeof(outname), "decoded_%s.txt", base);

    FILE *in  = fopen(bin, "rb");
    FILE *out = fopen(outname, "w");
    if (!in || !out) { fprintf(stderr, "Err %s\n", bin); return; }

    int tok;
    while (fread(&tok, sizeof(int), 1, in) == 1) {
        if (tok >= 0) fprintf(out, "%s", global_dict[tok].word);
        else          fputc(-tok, out);
    }
    fclose(in);
    fclose(out);

    printf("[Decode]     %-20s in %.4f s\n",
           bin, omp_get_wtime() - t0);
}

static void help(const char *p) {
    printf("Usage: %s [-t N] encode|decode files...\n", p);
}

int main(int argc, char *argv[]) {
    if (argc < 3) { help(argv[0]); return 1; }

    // Leer -t N
    int arg = 1;
    int threads = omp_get_max_threads();
    if (!strcmp(argv[arg], "-t")) {
        threads = atoi(argv[arg+1]);
        arg += 2;
    }
    omp_set_num_threads(threads);
    printf("OpenMP: using %d threads\n", threads);

    double t_start = omp_get_wtime();

    if (!strcmp(argv[arg], "encode")) {
        // 1) reservar diccionarios locales
        DictEntry **local_dicts = malloc(threads * sizeof(DictEntry*));
        int      *local_sizes = malloc(threads * sizeof(int));
        for (int t = 0; t < threads; t++) {
            local_dicts[t] = malloc(MAX_DICT_SIZE * sizeof(DictEntry));
            local_sizes[t] = 0;
        }

        // 2) paralelo: cada hilo codifica sus archivos en su dict local
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            DictEntry *loc = local_dicts[tid];
            int *lsz = &local_sizes[tid];

            #pragma omp for schedule(dynamic)
            for (int i = arg+1; i < argc; i++)
                encode_file(argv[i], loc, lsz);

            #pragma omp single
            {
                // 3) fusionar todos lo locals en global_dict
                for (int t = 0; t < threads; t++) {
                    for (int j = 0; j < local_sizes[t]; j++) {
                        DictEntry *e = &local_dicts[t][j];
                        if (dict_find(global_dict, global_size, e->word) < 0)
                            dict_add(global_dict, &global_size, e->word);
                    }
                }
                save_dict("dict.txt");
            }
        }

        // 4) liberar memoria local
        for (int t = 0; t < threads; t++) free(local_dicts[t]);
        free(local_dicts);
        free(local_sizes);

    } else if (!strcmp(argv[arg], "decode")) {
        load_dict("dict.txt");
        #pragma omp parallel for schedule(dynamic)
        for (int i = arg+1; i < argc; i++)
            decode_file(argv[i]);
    }
    else {
        help(argv[0]);
        return 1;
    }

    printf("Total wall time: %.4f s\n", omp_get_wtime() - t_start);
    return 0;
}