#include "save2.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "save.h"

#ifdef ARM9
#define S2_PATH "/_hds/game.sav"
#define S2_TMP  "/_hds/game.tmp"
#define S2_BAD  "/_hds/game.bad"
#else
// Harness de PC: archivos locales, sin libfat.
#define S2_PATH "_hds/game.sav"
#define S2_TMP  "_hds/game.tmp"
#define S2_BAD  "_hds/game.bad"
#endif

// Guards de layout (FASE3-INTEGRACION §2.1): el save es un dump directo del
// struct; DS (ARM EABI) y host (x86-64) comparten layout porque ningún miembro
// alinea a más de 4. Si alguno truena, el struct cambió: sube SAVE2_VERSION.
_Static_assert(sizeof(Character) == 68, "Character cambio de layout: sube SAVE2_VERSION");
_Static_assert(sizeof(Mission) == 52, "Mission cambio de layout: sube SAVE2_VERSION");
_Static_assert(sizeof(HappyEnding) == 44, "HappyEnding cambio de layout: sube SAVE2_VERSION");
_Static_assert(sizeof(GameState) == 2092, "GameState cambio de layout: sube SAVE2_VERSION");
_Static_assert(sizeof(Save2) == 2112, "Save2 cambio de layout: sube SAVE2_VERSION");

#define S2_CRC_LEN ((unsigned int)offsetof(Save2, crc))

static bool s2_quarantineFailed;

Save2LoadResult save2_load(Save2 *out) {
	memset(out, 0, sizeof *out);
	FILE *f = fopen(S2_PATH, "rb");
	if (!f) return SAVE2_ABSENT;

	Save2 d;
	size_t n = fread(&d, 1, sizeof d, f);
	fclose(f);

	Save2LoadResult r = SAVE2_BAD;
	if (n == sizeof d && d.magic == SAVE2_MAGIC) {
		if (d.version != SAVE2_VERSION) {
			r = SAVE2_OLD_VERSION;
		} else if (d.stateSize == sizeof(GameState)
		           && d.crc == save_crc32(&d, S2_CRC_LEN)) {
			*out = d;
			return SAVE2_OK;
		}
	}
	// Cuarentena: el archivo queda recuperable a mano en game.bad. Si el
	// rename falla, el caller debe pasar a modo sesión (sin escrituras)
	// para no destruir el original.
	remove(S2_BAD);
	s2_quarantineFailed = rename(S2_PATH, S2_BAD) != 0;
	return r;
}

bool save2_quarantine_failed(void) {
	return s2_quarantineFailed;
}

bool save2_write(Save2 *data) {
	data->magic = SAVE2_MAGIC;
	data->version = SAVE2_VERSION;
	data->stateSize = (uint16_t)sizeof(GameState);
	data->saveCount++;
	data->crc = save_crc32(data, S2_CRC_LEN);

	FILE *f = fopen(S2_TMP, "wb");
	if (!f) return false;
	size_t n = fwrite(data, 1, sizeof *data, f);
	fflush(f);
	fclose(f);
	if (n != sizeof *data) { remove(S2_TMP); return false; }

	// Atómico dentro de lo que FAT permite: el .sav viejo vive hasta el rename.
	remove(S2_PATH);
	return rename(S2_TMP, S2_PATH) == 0;
}
