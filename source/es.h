// Texto en español sobre la fuente default de libnds (layout CP437).
// La fuente ya trae á é í ó ú ñ Ñ ü ¿ ¡ É; este módulo solo convierte UTF-8
// a esos códigos. Á Í Ó Ú Ü no existen en CP437 y caen a A I O U U.
#ifndef ES_H
#define ES_H

#include <stddef.h>

// Convierte una cadena UTF-8 a bytes CP437 imprimibles por la consola.
// Escribe como máximo n-1 bytes + terminador. Devuelve la longitud escrita.
size_t es_convert(const char *utf8, char *out, size_t n);

// printf con conversión a CP437. Máximo 256 bytes por llamada.
void es_printf(const char *fmt, ...);

#endif
