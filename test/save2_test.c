// Harness de PC del save v2 (partida completa). Compilar y correr:
//   gcc -Wall -Wextra -o save2_test.exe test/save2_test.c
//       source/save2.c source/engine.c source/save.c  (una sola línea)
//   ./save2_test.exe
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../source/engine.h"
#include "../source/save2.h"
#include "../source/save.h"

#define PATH "_hds/game.sav"
#define BAD  "_hds/game.bad"

static void flip_byte(long offset) {
	FILE *f = fopen(PATH, "r+b");
	assert(f);
	fseek(f, offset, SEEK_SET);
	int c = fgetc(f);
	fseek(f, -1, SEEK_CUR);
	fputc(c ^ 0xFF, f);
	fclose(f);
}

static void make_state(GameState *s, GameDate today) {
	gameInit(s, today);
	uint16_t id, mid;
	CompleteResult cr;
	assert(createCharacter(s, "Ana", today, &id) == ENGINE_OK);
	assert(setAppearance(s, id, 1, 2, 3, 0) == ENGINE_OK);
	assert(createMission(s, id, "Correr 5k", DIFF_MEDIUM, today + 2, today, &mid) == ENGINE_OK);
	assert(completeMission(s, mid, today, &cr) == ENGINE_OK);
	assert(createCharacter(s, "Beto ñoño", today, &id) == ENGINE_OK);
	assert(createMission(s, id, "Leer", DIFF_EASY, today, today, &mid) == ENGINE_OK);
}

int main(void) {
	assert(save_init());
	remove(PATH);
	remove(BAD);

	const GameDate today = 20640;  // valor arbitrario, solo debe ser estable
	Save2 a, b;
	memset(&a, 0, sizeof a);
	make_state(&a.state, today);
	a.lastSeenDay = today;

	// 1. Sin archivo: primer arranque.
	assert(save2_load(&b) == SAVE2_ABSENT);

	// 2. Roundtrip: estado, lastSeenDay y saveCount sobreviven.
	assert(save2_write(&a));
	assert(a.saveCount == 1);
	assert(save2_write(&a));  // segunda escritura incrementa
	memset(&b, 0xAA, sizeof b);
	assert(save2_load(&b) == SAVE2_OK);
	assert(b.saveCount == 2);
	assert(b.lastSeenDay == today);
	assert(memcmp(&a.state, &b.state, sizeof a.state) == 0);

	// 3. Byte volteado en el estado -> corrupto, cuarentena en .bad.
	flip_byte(16 + 40);
	assert(save2_load(&b) == SAVE2_BAD);
	FILE *f = fopen(BAD, "rb");
	assert(f);
	fclose(f);
	assert(save2_load(&b) == SAVE2_ABSENT);  // el corrupto ya no estorba
	remove(BAD);

	// 4. Versión distinta -> OLD_VERSION (magic intacto), cuarentena.
	assert(save2_write(&a));
	flip_byte(4);
	assert(save2_load(&b) == SAVE2_OLD_VERSION);
	remove(BAD);

	// 5. Magic ajeno -> BAD.
	assert(save2_write(&a));
	flip_byte(0);
	assert(save2_load(&b) == SAVE2_BAD);
	remove(BAD);

	// 6. Archivo truncado -> BAD.
	assert(save2_write(&a));
	f = fopen(PATH, "rb");
	assert(f);
	char buf[64];
	size_t n = fread(buf, 1, sizeof buf, f);
	fclose(f);
	f = fopen(PATH, "wb");
	assert(f);
	fwrite(buf, 1, n, f);
	fclose(f);
	assert(save2_load(&b) == SAVE2_BAD);
	remove(BAD);

	// 7. Tras corrupción se puede reescribir y volver a cargar.
	assert(save2_write(&a));
	assert(save2_load(&b) == SAVE2_OK);
	assert(memcmp(&a.state, &b.state, sizeof a.state) == 0);
	remove(PATH);

	puts("SAVE2 TESTS OK");
	return 0;
}
