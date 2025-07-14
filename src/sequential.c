/*****************************************************************
 CompactText — versión C99 pura (encode / decode)
 · Usa uthash.h para el diccionario  (un único archivo, header-only)
   https://troydhanson.github.io/uthash/
 *****************************************************************/
#define _POSIX_C_SOURCE 200809L   /* clock_gettime, strdup */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "uthash.h"
#include "common.h"

/* ---------- estructura hash ---------- */
typedef struct {
    char    *word;   /* clave  (malloc)          */
    uint32_t id;     /* valor */
    UT_hash_handle hh;
} entry_t;

/* ---------- helper: time diff en segundos ---------- */
static double sec_since(struct timespec a, struct timespec b)
{
    return (b.tv_sec  - a.tv_sec) +
           (b.tv_nsec - a.tv_nsec)*1e-9;
}

/* ---------- helper: stem (dir/base) ----------------- */
char *stem_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *dot   = strrchr(path, '.');
    if (!dot || dot < path) dot = path + strlen(path); /* sin .ext */
    size_t len = dot - (slash ? slash+1 : path);
    char *out = malloc(len + 1);
    memcpy(out, slash ? slash+1 : path, len);
    out[len] = '\0';
    return out;
}

/* ---------- tokenizador sencillo -------------------- */
static int is_token_char(unsigned char c)
{
    return isalnum(c) || c=='\'';  /* letras, números, apóstrofe */
}

/* ---------- ENCODE ---------------------------------- */
double encode_file_seq(const char *path)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* leer archivo completo en memoria */
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 0.0; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { perror("fread"); fclose(f); free(buf); return 0.0; }
    fclose(f);

    /* diccionario */
    entry_t *dict = NULL, *e;
    uint32_t next_id = 1;

    /* ids dinámicos */
    uint32_t *ids = NULL;
    size_t    ids_cap = 0, ids_len = 0;

    /* escanear */
    long i = 0;
    while (i < sz) {
        while (i < sz && !is_token_char(buf[i])) ++i;
        long j = i;
        while (j < sz && is_token_char(buf[j])) ++j;
        if (j > i) {
            size_t len = j - i;
            char tmp[256];
            char *w = len < sizeof(tmp) ? tmp : malloc(len+1);
            memcpy(w, buf+i, len); w[len]='\0';

            HASH_FIND_STR(dict, w, e);
            if (!e) {
                e = malloc(sizeof(*e));
                e->word = strdup(w);
                e->id   = next_id++;
                HASH_ADD_KEYPTR(hh, dict, e->word, len, e);
            }
            if (w != tmp) free(w);

            /* push id */
            if (ids_len == ids_cap) {
                ids_cap = ids_cap ? ids_cap*2 : 1024;
                ids = realloc(ids, ids_cap * sizeof(uint32_t));
            }
            ids[ids_len++] = e->id;
        }
        i = j;
    }
    free(buf);

    /* archivos de salida */
    char *base = stem_of(path);
    /* vocab */
    {
        char fname[512]; sprintf(fname, "%s_vocab.bin", base);
        FILE *out = fopen(fname, "wb");
        uint32_t vs = HASH_COUNT(dict);
        fwrite(&vs, sizeof vs, 1, out);
        for (e=dict; e; e=e->hh.next) {
            uint32_t len = strlen(e->word);
            fwrite(&e->id , sizeof e->id , 1, out);
            fwrite(&len   , sizeof len  , 1, out);
            fwrite(e->word, len, 1, out);
        }
        fclose(out);
    }
    /* texto */
    {
        char fname[512]; sprintf(fname, "%s_texto.bin", base);
        FILE *out = fopen(fname, "wb");
        uint32_t cnt = ids_len;
        fwrite(&cnt, sizeof cnt, 1, out);
        fwrite(ids , sizeof(uint32_t), cnt, out);
        fclose(out);
    }

    /* liberar memoria */
    free(ids);
    entry_t *cur, *tmp;
    HASH_ITER(hh, dict, cur, tmp) { HASH_DEL(dict, cur); free(cur->word); free(cur); }
    free(base);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = sec_since(t0,t1);
    printf("[SEQ] Encoded \"%s\"  words=%zu  unique=%u  |  %.4f s\n",
           path, (size_t)ids_len, next_id-1, secs);
    return secs;
}

/* ---------- DECODE ---------------------------------- */
double decode_file_seq(const char *stem)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* vocab */
    char fname[512]; sprintf(fname, "%s_vocab.bin", stem);
    FILE *v = fopen(fname, "rb");
    if (!v){ perror(fname); return 0.0; }
    uint32_t vs; fread(&vs, sizeof vs, 1, v);
    char **vocab = calloc(vs+1, sizeof(char*));
    for (uint32_t i=0;i<vs;++i){
        uint32_t id,len; fread(&id,sizeof id,1,v); fread(&len,sizeof len,1,v);
        char *w = malloc(len+1); fread(w,len,1,v); w[len]='\0';
        vocab[id]=w;
    }
    fclose(v);

    /* texto */
    sprintf(fname, "%s_texto.bin", stem);
    FILE *t = fopen(fname, "rb");
    if(!t){ perror(fname); return 0.0; }
    uint32_t cnt; fread(&cnt,sizeof cnt,1,t);
    uint32_t *ids = malloc(cnt*sizeof(uint32_t));
    fread(ids,sizeof(uint32_t),cnt,t); fclose(t);

    /* reconstruir */
    sprintf(fname, "%s_recon.txt", stem);
    FILE *out = fopen(fname, "w");
    for (uint32_t i=0;i<cnt;++i){
        fputs(vocab[ids[i]], out);
        if(i+1<cnt) fputc(' ', out);
    }
    fclose(out);

    /* liberar */
    for (uint32_t i=1;i<=vs;++i) free(vocab[i]);
    free(vocab); free(ids);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = sec_since(t0,t1);
    printf("[SEQ] Decoded \"%s\"  words=%u  |  %.4f s\n", stem, cnt, secs);
    return secs;
}

/* ---------- HELP & MAIN ----------------------------- */
void help(void)
{
    puts("Usage:\n"
         "  compacttext_seq encode <file1.txt> <file2.txt> …\n"
         "  compacttext_seq decode <file1>     <file2>     …\n"
         "        (para decode NO pongas .txt)");
}

int main(int argc, char *argv[])
{
    if (argc < 3) { help(); return 1; }

    double total = 0.0;
    if (strcmp(argv[1], "encode") == 0) {
        for (int i=2;i<argc;++i) total += encode_file_seq(argv[i]);
        printf("=== SEQ encode total: %.4f s ===\n", total);

    } else if (strcmp(argv[1], "decode") == 0) {
        for (int i=2;i<argc;++i) total += decode_file_seq(argv[i]);
        printf("=== SEQ decode total: %.4f s ===\n", total);

    } else help();
    return 0;
}
