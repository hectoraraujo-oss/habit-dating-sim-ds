// Habit Dating Sim DS — Fase 1: cimientos de riesgo.
// Prueba las 3 piezas que pueden hundir el proyecto: RTC real, save en
// microSD (atómico, con CRC) y texto en español con acentos.
// Feo a propósito: aquí se prueba fontanería, no estética.
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "es.h"
#include "save.h"

static const char *DIAS[7] = {
	"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"
};
static const char *MESES[12] = {
	"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
	"agosto", "septiembre", "octubre", "noviembre", "diciembre"
};

static void draw_clock(void) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char fecha[64];
	snprintf(fecha, sizeof(fecha), "%s %d de %s de %d", DIAS[t->tm_wday],
	         t->tm_mday, MESES[t->tm_mon], 1900 + t->tm_year);
	es_printf("\x1b[7;0H%*s%s", (int)(32 - strlen(fecha)) / 2, "", fecha);
	es_printf("\x1b[9;12H%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}

int main(void) {
	PrintConsole top, bottom;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&top, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottom, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&top);
	es_printf("\x1b[1;8HHabit Dating Sim");
	es_printf("\x1b[2;13HFase 1");
	es_printf("\x1b[13;2HPrueba de fuente en español:");
	es_printf("\x1b[15;2Háéíóú ñÑ ü ¿? ¡! É");
	es_printf("\x1b[17;2HEl ñoño pingüino comió maíz");

	// Save: cargar, contar el encendido, escribir de vuelta.
	bool fatOk = save_init();
	SaveData sd;
	bool hadSave = fatOk && save_load(&sd);
	if (!fatOk || !hadSave) memset(&sd, 0, sizeof(sd));
	uint32_t prevBootTime = sd.lastBootTime;
	sd.bootCount++;
	sd.lastBootTime = (uint32_t)time(NULL);
	bool writeOk = fatOk && save_write(&sd);

	consoleSelect(&bottom);
	if (!fatOk) {
		es_printf("\x1b[1;1HmicroSD: NO disponible");
		es_printf("\x1b[3;1H(en emulador es normal sin");
		es_printf("\x1b[4;1HDLDI; en consola debe jalar)");
	} else {
		es_printf("\x1b[1;1HmicroSD: OK");
		es_printf("\x1b[2;1HEscritura: %s", writeOk ? "OK" : "ERROR");
		if (hadSave) {
			time_t prev = (time_t)prevBootTime;
			struct tm *p = localtime(&prev);
			es_printf("\x1b[3;1HSave previo válido (CRC OK)");
			es_printf("\x1b[4;1HÚltimo boot: %02d/%02d %02d:%02d",
			          p->tm_mday, p->tm_mon + 1, p->tm_hour, p->tm_min);
		} else {
			es_printf("\x1b[3;1HSave nuevo (primera vez)");
		}
	}
	es_printf("\x1b[7;1HEncendidos: %lu", (unsigned long)sd.bootCount);
	es_printf("\x1b[9;1HCorazones de prueba: %lu", (unsigned long)sd.testCounter);
	es_printf("\x1b[13;1HA = sumar corazón y guardar");
	es_printf("\x1b[15;1HApágalo y préndelo: debe");
	es_printf("\x1b[16;1Hrecordar los dos contadores.");

	consoleSelect(&top);
	draw_clock();

	time_t lastSec = time(NULL);
	while (1) {
		swiWaitForVBlank();
		scanKeys();

		if (keysDown() & KEY_A) {
			sd.testCounter++;
			bool ok = fatOk && save_write(&sd);
			consoleSelect(&bottom);
			es_printf("\x1b[9;1HCorazones de prueba: %lu ",
			          (unsigned long)sd.testCounter);
			es_printf("\x1b[11;1H%s", ok ? "[guardado en microSD]  "
			                             : "[SIN SD: no se guardó] ");
		}

		time_t now = time(NULL);
		if (now != lastSec) {
			lastSec = now;
			consoleSelect(&top);
			draw_clock();
		}
	}

	return 0;
}
