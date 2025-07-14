#ifndef COMPACTTEXT_COMMON_H
#define COMPACTTEXT_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double encode_file_seq (const char *path);
double decode_file_seq (const char *stem);
void   help(void);

/* Devuelve una nueva cadena con “dir/base” (sin .txt)  ──  el
   llamante debe liberar la memoria con free(). */
char  *stem_of(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* COMPACTTEXT_COMMON_H */
