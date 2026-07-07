#include "es.h"

#include <stdarg.h>
#include <stdio.h>

// Mapa de codepoints Unicode a CP437 (solo lo que usa el juego).
static char cp437_for(unsigned int cp) {
	switch (cp) {
	case 0x00E1: return (char)0xA0; // á
	case 0x00E9: return (char)0x82; // é
	case 0x00ED: return (char)0xA1; // í
	case 0x00F3: return (char)0xA2; // ó
	case 0x00FA: return (char)0xA3; // ú
	case 0x00F1: return (char)0xA4; // ñ
	case 0x00D1: return (char)0xA5; // Ñ
	case 0x00FC: return (char)0x81; // ü
	case 0x00C9: return (char)0x90; // É
	case 0x00BF: return (char)0xA8; // ¿
	case 0x00A1: return (char)0xAD; // ¡
	case 0x00B7: return (char)0xFA; // · (viñeta de misión pendiente)
	case 0x2665: return (char)0x03; // ♥ (corazón de la fuente CP437)
	case 0x2591: return (char)0xB0; // ░ (tramo vacío de barras)
	case 0x221A: return (char)0xFB; // √ (misión completada)
	// ponytail: CP437 no tiene Á Í Ó Ú Ü; caen a la vocal sin acento.
	// Si algún copy las necesita de verdad, se parchan 5 glifos en RAM.
	case 0x00C1: return 'A';
	case 0x00CD: return 'I';
	case 0x00D3: return 'O';
	case 0x00DA: return 'U';
	case 0x00DC: return 'U';
	default:     return '?';
	}
}

size_t es_convert(const char *utf8, char *out, size_t n) {
	if (n == 0) return 0;
	size_t o = 0;
	const unsigned char *s = (const unsigned char *)utf8;
	while (*s && o + 1 < n) {
		unsigned char b = *s++;
		if (b < 0x80) {
			out[o++] = (char)b;
		} else if ((b & 0xE0) == 0xC0 && (*s & 0xC0) == 0x80) {
			unsigned int cp = ((b & 0x1Fu) << 6) | (*s++ & 0x3Fu);
			out[o++] = cp437_for(cp);
		} else if ((b & 0xF0) == 0xE0 && (s[0] & 0xC0) == 0x80 &&
		           (s[1] & 0xC0) == 0x80) {
			// 3 bytes: aquí viven ♥ ░ √ y demás glifos de UI.
			unsigned int cp = ((b & 0x0Fu) << 12) |
			                  ((unsigned int)(s[0] & 0x3Fu) << 6) |
			                  (unsigned int)(s[1] & 0x3Fu);
			s += 2;
			out[o++] = cp437_for(cp);
		} else {
			// Secuencias de 4 bytes o malformadas: fuera del repertorio.
			while ((*s & 0xC0) == 0x80) s++;
			out[o++] = '?';
		}
	}
	out[o] = '\0';
	return o;
}

void es_printf(const char *fmt, ...) {
	char utf8[256];
	char cp437[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(utf8, sizeof(utf8), fmt, ap);
	va_end(ap);
	es_convert(utf8, cp437, sizeof(cp437));
	fputs(cp437, stdout);
	fflush(stdout);
}
