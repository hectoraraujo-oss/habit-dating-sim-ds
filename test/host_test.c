// Harness de PC para la lógica pura de Fase 1 (es.c y save.c).
// Compila con gcc nativo, corre con asserts. No toca libnds.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../source/es.h"
#include "../source/save.h"

static void test_es(void) {
	char out[64];

	es_convert("¿Dónde está el ñoño?", out, sizeof(out));
	assert((unsigned char)out[0] == 0xA8);        // ¿
	assert(strstr(out, "D\xA2nde") != NULL);      // ó
	assert(strstr(out, "est\xA0") != NULL);       // á
	assert(strstr(out, "\xA4o\xA4o") != NULL);    // ñoño

	es_convert("miércoles sábado pingüino", out, sizeof(out));
	assert(strstr(out, "mi\x82rcoles") != NULL);  // é
	assert(strstr(out, "s\xA0" "bado") != NULL);  // á
	assert(strstr(out, "ping\x81ino") != NULL);   // ü

	es_convert("¡É! Ñu", out, sizeof(out));
	assert((unsigned char)out[0] == 0xAD);        // ¡
	assert((unsigned char)out[1] == 0x90);        // É
	assert((unsigned char)out[4] == 0xA5);        // Ñ

	es_convert("ÁÍÓÚÜ", out, sizeof(out));
	assert(strcmp(out, "AIOUU") == 0);            // fallback sin acento

	es_convert("\xE4\xB8\xAD", out, sizeof(out)); // CJK de 3 bytes
	assert(strcmp(out, "?") == 0);

	// Truncado seguro: no desborda y termina en NUL.
	char tiny[4];
	size_t n = es_convert("ñandú", tiny, sizeof(tiny));
	assert(n == 3 && tiny[3] == '\0');
}

static void test_save(void) {
	SaveData a, b;
	memset(&a, 0, sizeof(a));

	assert(save_init());
	a.bootCount = 7;
	a.testCounter = 3;
	a.lastBootTime = 1234567u;
	assert(save_write(&a));
	assert(a.magic == SAVE_MAGIC && a.version == SAVE_VERSION);

	assert(save_load(&b));
	assert(b.bootCount == 7);
	assert(b.testCounter == 3);
	assert(b.lastBootTime == 1234567u);

	// Un byte corrupto debe tirar el CRC.
	FILE *f = fopen("_hds/fase1.sav", "r+b");
	assert(f != NULL);
	fseek(f, 8, SEEK_SET); // bootCount
	fputc(0xFF, f);
	fclose(f);
	assert(!save_load(&b));
	assert(b.bootCount == 0); // out queda en ceros si el save no es válido

	// Y un save fresco lo repara.
	assert(save_write(&a));
	assert(save_load(&b));
	assert(b.bootCount == 7);
}

int main(void) {
	test_es();
	test_save();
	puts("HOST TESTS OK");
	return 0;
}
