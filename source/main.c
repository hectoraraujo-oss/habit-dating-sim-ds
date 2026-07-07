// Habit Dating Sim DS — arranque.
// Fase 3: consolas, FAT, save v2 y control a la UI. La demo de Fase 1
// (RTC + contadores) cumplió su gate y fue reemplazada por el juego real.
#include <nds.h>

#include "save.h"
#include "save2.h"
#include "ui.h"

int main(void) {
	PrintConsole top, bottom;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&top, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottom, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	bool fatOk = save_init();
	static Save2 sv;  // ~2 KB: estático, fuera del stack
	Save2LoadResult r = fatOk ? save2_load(&sv) : SAVE2_ABSENT;

	ui_run(&sv, fatOk, r, &top, &bottom);
	return 0;
}
