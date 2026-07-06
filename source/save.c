#include "save.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef ARM9
#include <fat.h>
#define SAVE_DIR  "/_hds"
#define SAVE_PATH "/_hds/fase1.sav"
#define SAVE_TMP  "/_hds/fase1.tmp"
#else
// Harness de PC: archivos locales, sin libfat.
#define SAVE_DIR  "_hds"
#define SAVE_PATH "_hds/fase1.sav"
#define SAVE_TMP  "_hds/fase1.tmp"
#endif

uint32_t save_crc32(const void *buf, unsigned int len) {
	// ponytail: CRC32 bit a bit sin tabla; el save mide 24 bytes, sobra.
	const unsigned char *p = buf;
	uint32_t crc = 0xFFFFFFFFu;
	for (unsigned int i = 0; i < len; i++) {
		crc ^= p[i];
		for (int b = 0; b < 8; b++)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

#define CRC_LEN ((unsigned int)((char *)&((SaveData *)0)->crc - (char *)0))

bool save_init(void) {
#ifdef ARM9
	if (!fatInitDefault()) return false;
#endif
	mkdir(SAVE_DIR, 0777); // EEXIST es normal
	return true;
}

bool save_load(SaveData *out) {
	memset(out, 0, sizeof(*out));
	FILE *f = fopen(SAVE_PATH, "rb");
	if (!f) return false;
	SaveData d;
	size_t n = fread(&d, 1, sizeof(d), f);
	fclose(f);
	if (n != sizeof(d)) return false;
	if (d.magic != SAVE_MAGIC) return false;
	if (d.version != SAVE_VERSION) return false;
	if (d.crc != save_crc32(&d, CRC_LEN)) return false;
	*out = d;
	return true;
}

bool save_write(SaveData *data) {
	data->magic = SAVE_MAGIC;
	data->version = SAVE_VERSION;
	data->crc = save_crc32(data, CRC_LEN);

	FILE *f = fopen(SAVE_TMP, "wb");
	if (!f) return false;
	size_t n = fwrite(data, 1, sizeof(*data), f);
	fflush(f);
	fclose(f);
	if (n != sizeof(*data)) { remove(SAVE_TMP); return false; }

	// Atómico dentro de lo que FAT permite: el .sav viejo vive hasta el rename.
	remove(SAVE_PATH);
	if (rename(SAVE_TMP, SAVE_PATH) != 0) return false;
	return true;
}
