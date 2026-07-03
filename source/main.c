// Habit Dating Sim DS — Fase 0: hello world.
// Solo prueba la cadena de build y las dos pantallas. Sin acentos: la fuente
// en espanol es trabajo de Fase 1.
#include <nds.h>
#include <stdio.h>

int main(void) {
	PrintConsole top, bottom;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&top, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottom, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&top);
	iprintf("\n\n\n\n\n\n\n\n");
	iprintf("      Habit Dating Sim\n");
	iprintf("        Nintendo DS\n");

	consoleSelect(&bottom);
	iprintf("\n\n\n\n\n\n\n\n");
	iprintf("        Fase 0 OK\n\n");
	iprintf("   La cadena de build vive.\n");

	while (1) {
		swiWaitForVBlank();
	}

	return 0;
}
