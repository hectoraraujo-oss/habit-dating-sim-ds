// Habit Dating Sim DS — Fase 3: pantallas y UI.
// Arriba (no táctil) = contemplación. Abajo (táctil) = acción.
// Modo texto/consola con la fuente CP437 en español (Fase 1); los sprites
// llegan en Fase 4. Specs: FASE3-PANTALLAS.md y FASE3-INTEGRACION.md.
//
// ponytail: navegación por stylus + botón B (atrás) + X (ranking). El espejo
// completo de D-pad/A de la spec se agrega si el stylus resulta incómodo.
#include "ui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "es.h"
#include "fecha.h"
#include "save2.h"

// ---------------------------------------------------------------- estado

static PrintConsole *TOP, *BOT;
static Save2 *SV;
static GameState *G;
static bool g_fatOk;
static char g_toast[48];

// ---------------------------------------------------------------- fechas

static GameDate rtc_today(void) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	return fecha_from_civil(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

// El juego nunca retrocede: si el RTC quedó atrás de la última partida
// (pila de la consola, reloj mal puesto), rige la fecha guardada.
static GameDate today(void) {
	GameDate d = rtc_today();
	return d >= SV->lastSeenDay ? d : SV->lastSeenDay;
}

// ---------------------------------------------------------------- guardado

static void toast_set(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(g_toast, sizeof g_toast, fmt, ap);
	va_end(ap);
}

static void guardar(void) {
	GameDate hoy = today();
	if (SV->lastSeenDay < hoy) SV->lastSeenDay = hoy;
	if (g_fatOk && !save2_write(SV)) toast_set("¡No se pudo guardar!");
}

// ---------------------------------------------------------------- dibujo

static void pat(PrintConsole *c, int row, int col, const char *fmt, ...) {
	char buf[160];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	consoleSelect(c);
	es_printf("\x1b[%d;%dH%s", row, col, buf);
}

static int vislen(const char *utf8) {
	char tmp[160];
	return (int)es_convert(utf8, tmp, sizeof tmp);
}

// Centrado horizontal en 32 columnas.
static void patc(PrintConsole *c, int row, const char *fmt, ...) {
	char buf[160];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	int col = (32 - vislen(buf)) / 2;
	if (col < 0) col = 0;
	pat(c, row, col, "%s", buf);
}

static void cls(PrintConsole *c) {
	consoleSelect(c);
	printf("\x1b[2J");
}

// Copia utf8 truncado a maxw celdas visibles (corta en frontera de carácter)
// y opcionalmente rellena con espacios hasta maxw (columnas alineadas).
static const char *fitname(const char *utf8, int maxw, bool padded,
                           char *buf, size_t bufsz) {
	const unsigned char *s = (const unsigned char *)utf8;
	size_t o = 0;
	int w = 0;
	while (*s && w < maxw) {
		int len = (*s < 0x80) ? 1 : ((*s & 0xE0) == 0xC0) ? 2
		        : ((*s & 0xF0) == 0xE0) ? 3 : 4;
		if (o + (size_t)len >= bufsz) break;
		memcpy(buf + o, s, (size_t)len);
		o += (size_t)len;
		s += len;
		w++;
	}
	while (padded && w < maxw && o + 1 < bufsz) { buf[o++] = ' '; w++; }
	buf[o] = 0;
	return buf;
}

// Texto con salto de línea por palabra. Devuelve filas usadas.
static int wrap(PrintConsole *c, int row, int col, int w, const char *text) {
	char line[96] = "";
	char word[64];
	int rows = 0;
	const char *p = text;
	while (*p) {
		int wl = 0;
		while (*p && *p != ' ' && wl < (int)sizeof word - 1) word[wl++] = *p++;
		word[wl] = 0;
		while (*p == ' ') p++;
		char cand[96];
		if (line[0]) snprintf(cand, sizeof cand, "%s %s", line, word);
		else snprintf(cand, sizeof cand, "%s", word);
		if (vislen(cand) > w && line[0]) {
			pat(c, row + rows++, col, "%s", line);
			snprintf(line, sizeof line, "%s", word);
		} else {
			snprintf(line, sizeof line, "%s", cand);
		}
	}
	if (line[0]) pat(c, row + rows++, col, "%s", line);
	return rows;
}

// Barra de corazones: ancho fijo, ♥ lleno / ░ vacío.
static void barra_str(char *out, size_t n, int cur, int tot, int width) {
	int fill = tot > 0 ? (cur * width + tot - 1) / tot : 0;
	if (fill > width) fill = width;
	if (fill < 0) fill = 0;
	size_t o = 0;
	for (int i = 0; i < width; i++) {
		const char *g = i < fill ? "\x03" : "░";
		size_t gl = strlen(g);
		if (o + gl + 1 >= n) break;
		memcpy(out + o, g, gl);
		o += gl;
	}
	out[o] = 0;
}

// ---------------------------------------------------------------- zonas táctiles

#define INP_B (-2)
#define INP_X (-3)

typedef struct { int16_t val; uint8_t row, col, h, w; } Zone;

static Zone zs[34];
static int nz;

static void zreset(void) { nz = 0; }

static void zadd(int val, int row, int col, int h, int w) {
	if (nz >= (int)(sizeof zs / sizeof zs[0])) return;
	zs[nz++] = (Zone){ (int16_t)val, (uint8_t)row, (uint8_t)col,
	                   (uint8_t)h, (uint8_t)w };
}

// Botón con marco de h filas, etiqueta centrada, zona táctil incluida.
static void zbtn(int val, int row, int col, int w, int h, const char *label) {
	char line[40];
	if (w > 33) w = 33;
	line[0] = '+';
	memset(line + 1, '-', (size_t)(w - 2));
	line[w - 1] = '+';
	line[w] = 0;
	pat(BOT, row, col, "%s", line);
	pat(BOT, row + h - 1, col, "%s", line);
	for (int r = row + 1; r < row + h - 1; r++) {
		pat(BOT, r, col, "|");
		pat(BOT, r, col + w - 1, "|");
	}
	int lw = vislen(label);
	int lcol = col + (w - lw) / 2;
	if (lcol < col + 1) lcol = col + 1;
	pat(BOT, row + (h - 1) / 2, lcol, "%s", label);
	zadd(val, row, col, h, w);
}

// Espera tap sobre una zona o botón físico permitido.
static int zwait(uint32_t allowKeys) {
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		uint32_t down = keysDown();
		if ((allowKeys & KEY_B) && (down & KEY_B)) return INP_B;
		if ((allowKeys & KEY_X) && (down & KEY_X)) return INP_X;
		if (down & KEY_TOUCH) {
			touchPosition t;
			touchRead(&t);
			int col = t.px / 8, row = t.py / 8;
			for (int i = 0; i < nz; i++)
				if (col >= zs[i].col && col < zs[i].col + zs[i].w &&
				    row >= zs[i].row && row < zs[i].row + zs[i].h)
					return zs[i].val;
		}
	}
}

// ---------------------------------------------------------------- copy

static const char *ETAPAS[4] = {
	"Extraños", "Conocidos", "Amigos cercanos", "Amor consolidado"
};
static const char *DIFS[3] = { "Fácil", "Media", "Difícil" };
static const char *DIF_DESC[3] = {
	"Puedo hacerlo incluso cansado",
	"Requiere esfuerzo real",
	"Me va a costar, pero vale la pena",
};

// Etiqueta emocional (placeholder textual del sprite, Fase 3).
static const char *estado_de(const Character *c, GameDate hoy) {
	if (rankingLeader(G) == c->id) return "[orgulloso]";
	if (isAtRisk(c, hoy)) return "[triste]";
	return "[tranquilo]";
}

// ---------------------------------------------------------------- avisos

// Pantalla genérica de aviso con botón único.
static void aviso(const char *titulo, const char *cuerpo, const char *boton) {
	cls(TOP);
	cls(BOT);
	patc(TOP, 4, "%s", titulo);
	zreset();
	wrap(BOT, 3, 1, 30, cuerpo);
	zbtn(1, 17, 4, 24, 3, boton);
	zwait(0);
}

// ---------------------------------------------------------------- teclado

// Teclado táctil propio (grid de texto): A-Z + Ñ + espacio.
// ponytail: mayúsculas sin acentos bastan para nombrar hábitos; si un
// nombre necesita más, se amplía el grid, no se cambia el sistema.
static const char *KEYS[27] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I",
	"J", "K", "L", "M", "N", "Ñ", "O", "P", "Q",
	"R", "S", "T", "U", "V", "W", "X", "Y", "Z",
};

#define KB_MAX_W 20  // celdas visibles del campo (< NAME_LEN-1 bytes)

static bool teclado(const char *titulo, char *out, size_t outsz) {
	char buf[NAME_LEN];
	snprintf(buf, sizeof buf, "%s", out);
	while (1) {
		cls(BOT);
		pat(BOT, 1, 1, "%s", titulo);
		pat(BOT, 3, 1, "> %s_", buf);
		zreset();
		for (int i = 0; i < 27; i++) {
			int r = 6 + (i / 9) * 2, c = 2 + (i % 9) * 3;
			pat(BOT, r, c + 1, "%s", KEYS[i]);
			zadd(i, r, c, 2, 3);
		}
		zbtn(100, 13, 1, 11, 3, "ESPACIO");
		zbtn(101, 13, 13, 9, 3, "BORRAR");
		zbtn(102, 13, 23, 8, 3, "LISTO");
		pat(BOT, 17, 1, "B = cancelar");
		int v = zwait(KEY_B);
		if (v == INP_B) return false;
		size_t len = strlen(buf);
		if (v >= 0 && v < 27) {
			const char *k = KEYS[v];
			if (vislen(buf) < KB_MAX_W && len + strlen(k) < sizeof buf)
				strcat(buf, k);
		} else if (v == 100) {
			if (vislen(buf) < KB_MAX_W && len + 1 < sizeof buf && len > 0)
				strcat(buf, " ");
		} else if (v == 101 && len > 0) {
			// retrocede una posición UTF-8 completa (Ñ mide 2 bytes)
			size_t i = len - 1;
			while (i > 0 && (buf[i] & 0xC0) == 0x80) i--;
			buf[i] = 0;
		} else if (v == 102) {
			// sin espacios colgantes al final
			len = strlen(buf);
			while (len > 0 && buf[len - 1] == ' ') buf[--len] = 0;
			if (len > 0) {
				snprintf(out, outsz, "%s", buf);
				return true;
			}
			pat(BOT, 17, 15, "¡Ponle nombre!");
		}
	}
}

// ---------------------------------------------------------------- escenas

// V5: subida de nivel (y §V5 variante boda abajo).
static void esc_nivel(const Character *c, const CompleteResult *res) {
	GameDate hoy = today();
	char nom[40];
	cls(TOP);
	cls(BOT);
	patc(TOP, 6, "+------------------------+");
	for (int r = 7; r < 13; r++) patc(TOP, r, "|                        |");
	patc(TOP, 13, "+------------------------+");
	patc(TOP, 9, "%s", fitname(c->name, 22, false, nom, sizeof nom));
	patc(TOP, 11, "[feliz]");
	char linea[96];
	snprintf(linea, sizeof linea, "Llevas %ld días cuidándome. No lo olvidaré.",
	         (long)daysTogether(c, hoy));
	wrap(BOT, 2, 1, 30, linea);
	patc(BOT, 6, "Nivel %d alcanzado:", res->newLevel);
	patc(BOT, 7, "%s", ETAPAS[res->newLevel]);
	patc(BOT, 10, "%ld días juntos · %u misiones",
	     (long)daysTogether(c, hoy), c->completedCount);
	zreset();
	zbtn(1, 17, 4, 24, 3, "CONTINUAR");
	zwait(0);
}

// V5 variante boda (D2). Copy propuesto, pendiente de revisión de Hector (A1).
static void esc_boda(const Character *c) {
	GameDate hoy = today();
	char nom[40];
	cls(TOP);
	cls(BOT);
	patc(TOP, 4, "♥  ♥  ♥");
	patc(TOP, 6, "+------------------------+");
	for (int r = 7; r < 13; r++) patc(TOP, r, "|                        |");
	patc(TOP, 13, "+------------------------+");
	patc(TOP, 9, "%s", fitname(c->name, 22, false, nom, sizeof nom));
	patc(TOP, 11, "[feliz]");
	patc(TOP, 15, "B O D A");
	wrap(BOT, 2, 1, 30,
	     "Lo logramos. Esto ya no es una promesa: es parte de quién eres. "
	     "Sí, acepto.");
	patc(BOT, 8, "%ld días juntos · %u misiones",
	     (long)daysTogether(c, hoy), c->completedCount);
	patc(BOT, 9, "%d ♥ de recuerdo", c->heartsTotal);
	patc(BOT, 11, "Este slot ahora está libre.");
	zreset();
	zbtn(1, 17, 4, 24, 3, "CONTINUAR");
	zwait(0);
}

// V6: abandono. seFue=true → salida definitiva; false → solo bajó de nivel.
static void esc_abandono(const Character *c, bool seFue) {
	GameDate hoy = today();
	char nom[40];
	fitname(c->name, 20, false, nom, sizeof nom);
	cls(TOP);
	cls(BOT);
	patc(TOP, 6, "+------------------------+");
	for (int r = 7; r < 13; r++) patc(TOP, r, "|                        |");
	patc(TOP, 13, "+------------------------+");
	patc(TOP, 9, "%s", nom);
	patc(TOP, 11, seFue ? "[se va]" : "[triste]");
	zreset();
	if (seFue) {
		if (c->completedCount == 0)
			wrap(BOT, 1, 1, 30,
			     "Nos conocimos poco. Quizá no era el momento. Cuídate.");
		else
			wrap(BOT, 1, 1, 30,
			     "Esperé 21 días. Creo que ambos sabemos cómo terminó "
			     "esto. Cuídate.");
		char linea[96];
		snprintf(linea, sizeof linea, "Tu relación con %s duró %ld días.",
		         nom, (long)daysTogether(c, hoy));
		wrap(BOT, 6, 1, 30, linea);
		pat(BOT, 9, 1, "Llegaste al Nivel %d", c->level);
		pat(BOT, 10, 1, "Completaste %u misiones", c->completedCount);
		patc(BOT, 12, "Este slot ahora está libre.");
		zbtn(1, 17, 2, 28, 3, "Cerrar este capítulo");
	} else {
		char linea[96];
		snprintf(linea, sizeof linea, "%s se enfrió: bajó a Nivel %d.",
		         nom, c->level);
		wrap(BOT, 3, 1, 30, linea);
		wrap(BOT, 7, 1, 30, "Complétale una misión pronto o seguirá bajando.");
		zbtn(1, 17, 4, 24, 3, "Entendido");
	}
	zwait(0);
}

// V7: cancelación / pérdida (en sesión y re-hidratada al arranque).
static void esc_cancelacion(const Character *c, const char *misionNombre,
                            uint8_t dif, int16_t pen, bool dejasteIr) {
	char nom[40], mis[40];
	fitname(c->name, 20, false, nom, sizeof nom);
	fitname(misionNombre, 18, false, mis, sizeof mis);
	cls(TOP);
	cls(BOT);
	patc(TOP, 6, "+------------------------+");
	for (int r = 7; r < 13; r++) patc(TOP, r, "|                        |");
	patc(TOP, 13, "+------------------------+");
	patc(TOP, 9, "%s", nom);
	patc(TOP, 11, "[decepción]");
	wrap(BOT, 1, 1, 30,
	     "Okay. Lo entiendo. Pero ya sabes lo que cuesta esto.");
	pat(BOT, 5, 1, "%s: %s", dejasteIr ? "Dejaste ir" : "Cancelaste", mis);
	pat(BOT, 6, 1, "Dificultad: %s", DIFS[dif]);
	patc(BOT, 8, "-%d ♥", pen);
	int16_t cur, tot;
	char bar[40];
	if (heartsToNextLevel(c->level, c->heartsTotal, &cur, &tot)) {
		barra_str(bar, sizeof bar, cur, tot, 10);
		patc(BOT, 10, "%s %d/%d ♥", bar, cur, tot);
	}
	zreset();
	if (c->heartsTotal == 0) {
		char linea[96];
		snprintf(linea, sizeof linea,
		         "Si dejas de atender a %s, podría irse.", nom);
		wrap(BOT, 12, 1, 30, linea);
		zbtn(1, 16, 1, 30, 3, "Entendido. No lo olvidaré.");
	} else {
		zbtn(1, 17, 4, 24, 3, "ENTENDIDO");
	}
	zwait(0);
}

// Cierre de semana al arranque (P12): solo cuando hubo ganador.
static void esc_resultado_semana(uint16_t winnerId) {
	Character *c = findCharacter(G, winnerId);
	if (!c) return;
	char nom[40];
	fitname(c->name, 20, false, nom, sizeof nom);
	cls(TOP);
	cls(BOT);
	patc(TOP, 4, "RESULTADO DE LA SEMANA");
	patc(TOP, 8, "#1  %s  #1", nom);
	patc(TOP, 10, "[orgulloso]");
	char linea[96];
	snprintf(linea, sizeof linea, "¡%s ganó la semana!", nom);
	wrap(BOT, 3, 1, 30, linea);
	pat(BOT, 7, 1, "Semanas ganadas: %u", c->weeksWon);
	zreset();
	zbtn(1, 17, 4, 24, 3, "CONTINUAR");
	zwait(0);
}

// ---------------------------------------------------------------- creador (V8)

static uint16_t scr_creador(bool primera) {
	// firstFreeSlot lo garantiza el caller; aquí solo el flujo.
	int val[4] = { 0, 0, 0, 0 };            // base, color, accesorio, expresión
	const int tope[4] = { 4, 4, 5, 3 };     // accesorio: 0=ninguno, 1..4
	const char *eje[4] = { "Base", "Color", "Accesorio", "Expresión" };
	char nombre[NAME_LEN] = "";

	while (1) {
		cls(TOP);
		cls(BOT);
		patc(TOP, 2, primera ? "TU PRIMER HABITER" : "NUEVO HABITER");
		for (int i = 0; i < 4; i++) {
			char ej[24], lbl[24];
			fitname(eje[i], 10, true, ej, sizeof ej);
			if (i == 2 && val[2] == 0)
				snprintf(lbl, sizeof lbl, "ninguno");
			else
				snprintf(lbl, sizeof lbl, "%d de %d",
				         (i == 2) ? val[i] : val[i] + 1, (i == 2) ? 4 : tope[i]);
			pat(TOP, 6 + i * 2, 4, "%s %s", ej, lbl);
		}
		if (nombre[0]) patc(TOP, 15, "\"%s\"", nombre);
		patc(TOP, 18, "(el sprite llega en Fase 4)");

		zreset();
		for (int i = 0; i < 4; i++) {
			int r = 1 + i * 3;
			char lbl[40];
			if (i == 2 && val[2] == 0)
				snprintf(lbl, sizeof lbl, "%s: ninguno", eje[i]);
			else
				snprintf(lbl, sizeof lbl, "%s %d de %d", eje[i],
				         (i == 2) ? val[i] : val[i] + 1, (i == 2) ? 4 : tope[i]);
			pat(BOT, r, 1, "<");
			pat(BOT, r, 30, ">");
			patc(BOT, r, "%s", lbl);
			zadd(10 + i * 2, r - 1, 0, 3, 6);        // flecha izquierda
			zadd(11 + i * 2, r - 1, 26, 3, 6);       // flecha derecha
		}
		char nomz[40];
		pat(BOT, 13, 1, "Nombre: %s",
		    nombre[0] ? fitname(nombre, 20, false, nomz, sizeof nomz)
		              : "[tap para escribir]");
		zadd(30, 12, 0, 3, 32);
		zbtn(31, 16, 9, 14, 3, "CREAR");
		if (!primera) pat(BOT, 21, 1, "B = volver");

		int v = zwait(primera ? 0 : KEY_B);
		if (v == INP_B) return 0;
		if (v >= 10 && v <= 17) {
			int i = (v - 10) / 2;
			int dir = (v % 2) ? 1 : -1;
			val[i] = (val[i] + dir + tope[i]) % tope[i];
		} else if (v == 30) {
			teclado("Nombre de tu habiter:", nombre, sizeof nombre);
		} else if (v == 31) {
			if (!nombre[0]) {
				pat(BOT, 20, 1, "Ponle nombre a tu habiter");
				continue;
			}
			uint16_t id = 0;
			if (createCharacter(G, nombre, today(), &id) != ENGINE_OK)
				return 0;  // sin slot libre: no debería pasar (caller valida)
			setAppearance(G, id, (uint8_t)val[0], (uint8_t)val[1],
			              (uint8_t)val[2], (uint8_t)val[3]);
			guardar();
			return id;
		}
	}
}

// ---------------------------------------------------------------- misiones

// Lista de pendientes global ordenada: vencidas primero, luego deadline asc.
static int pendientes_ordenadas(uint16_t *out, int max, GameDate hoy) {
	int n = 0;
	for (int i = 0; i < MAX_MISSIONS && n < max; i++)
		if (G->missions[i].id && G->missions[i].status == MISSION_PENDING)
			out[n++] = G->missions[i].id;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++) {
			Mission *a = findMission(G, out[i]), *b = findMission(G, out[j]);
			bool va = a->deadline < hoy, vb = b->deadline < hoy;
			if ((vb && !va) || (va == vb && b->deadline < a->deadline)) {
				uint16_t t = out[i]; out[i] = out[j]; out[j] = t;
			}
		}
	return n;
}

static void fecha_rel(GameDate d, GameDate hoy, char *out, size_t n) {
	if (d == hoy) snprintf(out, n, "hoy");
	else if (d < hoy) snprintf(out, n, "hace %ldd", (long)(hoy - d));
	else if (d == hoy + 1) snprintf(out, n, "mañana");
	else fecha_format_corta(d, out, (unsigned)n);
}

// V3: crear misión.
static void scr_crear_mision(uint16_t charId) {
	Character *c = findCharacter(G, charId);
	if (!c) return;
	GameDate hoy = today();
	char nom[40];
	fitname(c->name, 20, false, nom, sizeof nom);

	if (pendingMissionCount(G, charId) >= MAX_PENDING_PER_CHARACTER) {
		char msg[144];
		snprintf(msg, sizeof msg,
		         "Ya tienes 3 misiones activas con %s. "
		         "Completa alguna antes de crear otra.", nom);
		aviso("Sin espacio", msg, "Entendido");
		return;
	}

	char nombre[NAME_LEN] = "";
	int dif = DIFF_MEDIUM;
	GameDate dl = hoy + 3;

	while (1) {
		cls(TOP);
		cls(BOT);
		patc(TOP, 2, "%s", nom);
		patc(TOP, 3, "%s", estado_de(c, hoy));
		patc(TOP, 8, "Si completas esto ganarás:");
		patc(TOP, 10, "+%d ♥", heartsForDifficulty((Difficulty)dif));
		wrap(TOP, 14, 2, 28,
		     "Cambiar la fecha límite después penaliza corazones.");

		zreset();
		char nomz[40];
		pat(BOT, 1, 1, "Nombre: %s",
		    nombre[0] ? fitname(nombre, 20, false, nomz, sizeof nomz)
		              : "¿Qué vas a hacer?");
		zadd(1, 0, 0, 3, 32);
		pat(BOT, 4, 1, "Dificultad:");
		for (int i = 0; i < 3; i++) {
			char lbl[16];
			snprintf(lbl, sizeof lbl, "%s%s", i == dif ? ">" : " ", DIFS[i]);
			zbtn(10 + i, 5, i * 11, 10, 3, lbl);
		}
		wrap(BOT, 9, 1, 30, DIF_DESC[dif]);
		char f[20];
		fecha_rel(dl, hoy, f, sizeof f);
		pat(BOT, 12, 1, "Fecha límite:");
		pat(BOT, 13, 1, "<");
		pat(BOT, 13, 30, ">");
		patc(BOT, 13, "%s (+%ldd)", f, (long)(dl - hoy));
		zadd(20, 12, 0, 3, 6);
		zadd(21, 12, 26, 3, 6);
		zbtn(30, 16, 2, 28, 3, "Confirmar misión");
		pat(BOT, 21, 1, "B = cancelar");

		int v = zwait(KEY_B);
		if (v == INP_B) return;
		if (v == 1) teclado("¿Qué vas a hacer?", nombre, sizeof nombre);
		else if (v >= 10 && v <= 12) dif = v - 10;
		else if (v == 20 && dl > hoy) dl--;          // P5: mínimo hoy
		else if (v == 21 && dl < hoy + 14) dl++;
		else if (v == 30) {
			if (!nombre[0]) {
				pat(BOT, 20, 1, "Dale un nombre a esta misión");
				continue;
			}
			// fecha fresca: la consola pudo pasar días abierta en esta pantalla
			GameDate ahora = today();
			if (dl < ahora) dl = ahora;
			uint16_t mid;
			EngineError e = createMission(G, charId, nombre, (Difficulty)dif,
			                              dl, ahora, &mid);
			if (e == ENGINE_OK) {
				guardar();
				toast_set("¡Misión creada!");
			} else {
				toast_set("No se pudo crear (%d)", (int)e);
			}
			return;
		}
	}
}

// V4: marcar completa (con variante vencida P4). true si mutó estado.
static bool scr_mision(uint16_t missionId) {
	Mission *m = findMission(G, missionId);
	if (!m || m->status != MISSION_PENDING) return false;
	Character *c = findCharacter(G, m->characterId);
	if (!c) return false;
	GameDate hoy = today();
	bool vencida = m->deadline < hoy;
	char nom[40], mis[40], f[20];
	fitname(c->name, 20, false, nom, sizeof nom);
	fitname(m->name, 26, false, mis, sizeof mis);
	fecha_rel(m->deadline, hoy, f, sizeof f);

	cls(TOP);
	cls(BOT);
	patc(TOP, 2, "%s", nom);
	patc(TOP, 3, "[esperanzado]");

	pat(BOT, 0, 1, "%s", mis);
	zreset();
	int16_t ganancia = calcHeartsEarned((Difficulty)m->difficulty,
	                                    m->deadline, hoy);
	int16_t pen = cancelPenaltyFor((Difficulty)m->difficulty);
	if (!vencida) {
		pat(BOT, 1, 1, "%s · vence %s", DIFS[m->difficulty], f);
		zbtn(1, 4, 1, 30, 5, "√ LO HICE");
		patc(BOT, 10, "Ganarás +%d ♥ al confirmar", ganancia);
		patc(BOT, 15, "cancelar esta misión (-%d ♥)", pen);
		zadd(2, 14, 0, 3, 32);
	} else {
		pat(BOT, 1, 1, "%s · ¡venció %s!", DIFS[m->difficulty], f);
		wrap(BOT, 3, 1, 30, "Llegó tarde, pero vale la pena cumplir. "
		     "Ganarás menos corazones.");
		char lbl[36];
		snprintf(lbl, sizeof lbl, "Sí lo hice (tarde) +%d ♥", ganancia);
		zbtn(1, 7, 1, 30, 4, lbl);
		snprintf(lbl, sizeof lbl, "Aceptar la pérdida (-%d ♥)", pen);
		zbtn(3, 13, 1, 30, 4, lbl);
	}
	pat(BOT, 21, 1, "B = volver");

	int v = zwait(KEY_B);
	if (v == INP_B) return false;
	if (v == 1) {
		CompleteResult res;
		// fecha fresca (no la de entrada): P4 y el ancla de inactividad
		// deben usar el día real de la acción
		if (completeMission(G, missionId, today(), &res) != ENGINE_OK)
			return false;
		guardar();
		if (res.wedding) {
			esc_boda(c);
			c->milestonesShown |= 0x08;  // boda confirmada en pantalla
			guardar();
		} else if (res.leveledUp) {
			esc_nivel(c, &res);
			c->milestonesShown |= (uint8_t)(1u << res.newLevel);
			guardar();
		} else {
			toast_set("¡Misión completada! +%d ♥", res.heartsEarned);
		}
		return true;
	}
	if (v == 2 || v == 3) {
		int16_t penalti = 0;
		char nombreMision[NAME_LEN];
		snprintf(nombreMision, sizeof nombreMision, "%s", m->name);
		uint8_t d = m->difficulty;
		EngineError e = (v == 3) ? acceptMissionLoss(G, missionId, &penalti)
		                         : cancelMission(G, missionId, &penalti);
		if (e != ENGINE_OK) return false;
		esc_cancelacion(c, nombreMision, d, penalti, v == 3);
		acknowledgeCancellationScene(G, c->id);
		guardar();
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- perfil (V2)

static void scr_perfil(uint16_t charId) {
	int page = 0;
	while (1) {
		Character *c = findCharacter(G, charId);
		if (!c || c->status != CHAR_ACTIVE) return;  // se casó o se fue
		GameDate hoy = today();
		char nom[40], buf[40];
		fitname(c->name, 20, false, nom, sizeof nom);

		cls(TOP);
		cls(BOT);
		patc(TOP, 1, "%s", nom);
		patc(TOP, 2, "%s", estado_de(c, hoy));
		patc(TOP, 4, "Nivel %d: %s", c->level, ETAPAS[c->level]);
		int16_t cur, tot;
		if (heartsToNextLevel(c->level, c->heartsTotal, &cur, &tot)) {
			char bar[40];
			barra_str(bar, sizeof bar, cur, tot, 10);
			patc(TOP, 6, "%s", bar);
			patc(TOP, 7, "%d/%d ♥ para Nivel %d", cur, tot, c->level + 1);
		}
		fecha_format_corta(c->createdDate, buf, sizeof buf);
		patc(TOP, 9, "Juntos desde: %s", buf);
		patc(TOP, 10, "%ld días juntos", (long)daysTogether(c, hoy));
		if (isAtRisk(c, hoy)) {
			char linea[144];
			snprintf(linea, sizeof linea,
			         "Tu relación con %s lleva %ld días sin misiones. "
			         "Tiene %ld días antes de que se vaya.",
			         nom, (long)daysInactive(c, hoy),
			         (long)daysUntilLeaving(daysInactive(c, hoy)));
			wrap(TOP, 13, 2, 28, linea);
		}

		// lista: pendientes primero (tappables), cerradas después
		uint16_t ids[MAX_MISSIONS];
		int n = 0, ncanceladas = 0, ncompletadas = 0;
		for (int i = 0; i < MAX_MISSIONS; i++) {
			Mission *mm = &G->missions[i];
			if (!mm->id || mm->characterId != charId) continue;
			if (mm->status == MISSION_COMPLETED) ncompletadas++;
			if (mm->status == MISSION_CANCELLED || mm->status == MISSION_FAILED)
				ncanceladas++;
			if (mm->status == MISSION_PENDING) ids[n++] = mm->id;
		}
		for (int i = 0; i < MAX_MISSIONS; i++) {  // cerradas, más nueva primero
			Mission *mm = &G->missions[i];
			if (mm->id && mm->characterId == charId &&
			    mm->status != MISSION_PENDING)
				ids[n++] = mm->id;
		}

		zreset();
		pat(BOT, 0, 1, "√ %u misiones   x %d", c->completedCount, ncanceladas);
		(void)ncompletadas;
		int totalPages = (n + 3) / 4;
		if (totalPages == 0) totalPages = 1;
		if (page >= totalPages) page = totalPages - 1;
		if (n == 0) {
			wrap(BOT, 3, 1, 30, "Crea tu primera misión. Las relaciones se "
			     "construyen con acciones, no con intenciones.");
		}
		for (int k = 0; k < 4; k++) {
			int idx = page * 4 + k;
			if (idx >= n) break;
			Mission *mm = findMission(G, ids[idx]);
			const char *st = mm->status == MISSION_PENDING
			                     ? (mm->deadline < hoy ? "!" : "·")
			                     : (mm->status == MISSION_COMPLETED ? "√" : "x");
			char mn[40], fr[20];
			fitname(mm->name, 14, true, mn, sizeof mn);
			int row = 2 + k * 2;
			if (mm->status == MISSION_PENDING) {
				fecha_rel(mm->deadline, hoy, fr, sizeof fr);
				pat(BOT, row, 1, "%s %s %c %s", st, mn,
				    "FMD"[mm->difficulty], fr);
				zadd(40 + idx, row, 0, 2, 32);
			} else {
				pat(BOT, row, 1, "%s %s %+d ♥", st, mn, mm->heartsAwarded);
			}
		}
		if (totalPages > 1) {
			pat(BOT, 10, 2, "< pag");
			pat(BOT, 10, 24, "pag >");
			pat(BOT, 10, 13, "%d/%d", page + 1, totalPages);
			zadd(60, 10, 0, 2, 8);
			zadd(61, 10, 22, 2, 10);
		}
		if (pendingMissionCount(G, charId) < MAX_PENDING_PER_CHARACTER)
			zbtn(62, 13, 1, 30, 3, "+ Crear nueva misión");
		else
			wrap(BOT, 14, 1, 30, "3 misiones activas: completa alguna "
			     "para crear otra.");
		pat(BOT, 21, 1, "B = volver");

		int v = zwait(KEY_B);
		if (v == INP_B) return;
		if (v == 60 && page > 0) page--;
		else if (v == 61 && page < totalPages - 1) page++;
		else if (v == 62) scr_crear_mision(charId);
		else if (v >= 40) scr_mision(ids[v - 40]);
	}
}

// ---------------------------------------------------------------- ranking (V9)

// Orden de podio (espejo del desempate del motor): weeklyHearts desc,
// más recientemente activo, creado antes, slot menor.
static int ranking_orden(Character **out) {
	int n = 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
		if (G->characters[i].id && G->characters[i].status == CHAR_ACTIVE &&
		    n < MAX_CHARACTERS)
			out[n++] = &G->characters[i];
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++) {
			Character *a = out[i], *b = out[j];
			bool swap = b->weeklyHearts > a->weeklyHearts ||
			    (b->weeklyHearts == a->weeklyHearts &&
			     (b->inactivitySince > a->inactivitySince ||
			      (b->inactivitySince == a->inactivitySince &&
			       (b->createdDate < a->createdDate ||
			        (b->createdDate == a->createdDate &&
			         b->slotNumber < a->slotNumber)))));
			if (swap) { out[i] = b; out[j] = a; }
		}
	return n;
}

static void scr_ranking(void) {
	GameDate hoy = today();
	GameDate ws = weekStart(hoy);
	Character *orden[MAX_CHARACTERS];
	int n = ranking_orden(orden);
	uint16_t lider = rankingLeader(G);
	char f1[20], f2[20], nom[40];
	fecha_format_corta(ws, f1, sizeof f1);
	fecha_format_corta(ws + 6, f2, sizeof f2);

	cls(TOP);
	cls(BOT);
	patc(TOP, 1, "Semana: %s - %s", f1, f2);
	if (n == 0) {
		patc(TOP, 8, "Sin concursantes todavía.");
	} else if (n == 1) {
		fitname(orden[0]->name, 18, false, nom, sizeof nom);
		patc(TOP, 6, "Tu semana con %s:", nom);
		patc(TOP, 9, "%+d ♥", orden[0]->weeklyHearts);
	} else {
		if (lider) {
			Character *lc = findCharacter(G, lider);
			fitname(lc->name, 16, false, nom, sizeof nom);
			patc(TOP, 3, "¡%s va arrasando!", nom);
		} else {
			wrap(TOP, 3, 3, 26, "Semana sin movimiento, nadie destacó");
		}
		// podio: #1 al centro, #2 y #3 a los lados; solo con líder real
		// (netos > 0), si no el "podio" premiaría semanas en ceros
		if (lider) {
			static const int pcol[3] = { 11, 0, 22 };
			static const int prow[3] = { 7, 9, 9 };
			for (int i = 0; i < n && i < 3; i++) {
				int cc = pcol[i], rr = prow[i];
				pat(TOP, rr, cc, "+--------+");
				pat(TOP, rr + 1, cc, "|   #%d   |", i + 1);
				fitname(orden[i]->name, 8, true, nom, sizeof nom);
				pat(TOP, rr + 2, cc, "|%s|", nom);
				pat(TOP, rr + 3, cc, "|%+4d ♥  |", orden[i]->weeklyHearts);
				pat(TOP, rr + 4, cc, "+--------+");
			}
			patc(TOP, 15, "LIDER DE LA SEMANA");
		}
	}

	pat(BOT, 0, 1, "Semana en curso");
	pat(BOT, 2, 1, "#  Nombre        sem  ganadas");
	for (int i = 0; i < n; i++) {
		fitname(orden[i]->name, 12, true, nom, sizeof nom);
		pat(BOT, 3 + i, 1, "%d  %s %+4d  %6u", i + 1, nom,
		    orden[i]->weeklyHearts, orden[i]->weeksWon);
	}
	if (G->lastWeekWinnerId) {
		Character *w = findCharacter(G, G->lastWeekWinnerId);
		if (w) {
			char linea[96];
			fitname(w->name, 16, false, nom, sizeof nom);
			snprintf(linea, sizeof linea,
			         "La semana pasada ganó: %s", nom);
			wrap(BOT, 8, 1, 30, linea);
		}
	}
	zreset();
	zbtn(1, 17, 4, 24, 3, "Volver");
	pat(BOT, 21, 1, "B = volver");
	zwait(KEY_B);
}

// ---------------------------------------------------------------- home (V1)

static void scr_inicio(void) {
	cls(TOP);
	cls(BOT);
	patc(TOP, 6, "+----------------------+");
	patc(TOP, 7, "|   HABIT DATING SIM   |");
	patc(TOP, 8, "+----------------------+");
	patc(TOP, 11, "Tus hábitos quieren");
	patc(TOP, 12, "una relación seria.");
	zreset();
	zbtn(1, 9, 3, 26, 4, "INICIAR PARTIDA");
	if (!g_fatOk)
		wrap(BOT, 16, 1, 30,
		     "Sin microSD: hoy se puede jugar pero nada se guardará.");
	zwait(0);
}

static void home_draw(GameDate hoy, int page, int *totalPages) {
	Character *slot[MAX_CHARACTERS] = { 0 };
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &G->characters[i];
		if (c->id && c->status == CHAR_ACTIVE && c->slotNumber >= 1 &&
		    c->slotNumber <= MAX_CHARACTERS)
			slot[c->slotNumber - 1] = c;
	}

	cls(TOP);
	cls(BOT);
	// ---- arriba: contemplación
	patc(TOP, 0, "HABIT DATING SIM");
	char fbuf[40], nom[40];
	int y, mo, d;
	fecha_to_civil(hoy, &y, &mo, &d);
	snprintf(fbuf, sizeof fbuf, "%s, %d de %s",
	         fecha_dia_nombre(fecha_weekday(hoy)), d, fecha_mes_nombre(mo));
	if (fbuf[0] >= 'a' && fbuf[0] <= 'z') fbuf[0] -= 32;
	patc(TOP, 2, "%s", fbuf);

	// destacado: el en-riesgo si existe; si no, el más recientemente activo
	Character *destacado = 0;
	for (int i = 0; i < MAX_CHARACTERS; i++)
		if (slot[i] && isAtRisk(slot[i], hoy)) destacado = slot[i];
	if (!destacado)
		for (int i = 0; i < MAX_CHARACTERS; i++)
			if (slot[i] && (!destacado ||
			    slot[i]->inactivitySince > destacado->inactivitySince))
				destacado = slot[i];

	if (destacado) {
		patc(TOP, 5, "%s", fitname(destacado->name, 20, false, nom, sizeof nom));
		patc(TOP, 6, "%s", estado_de(destacado, hoy));
		patc(TOP, 8, "Nivel %d: %s", destacado->level, ETAPAS[destacado->level]);
		int16_t cur, tot;
		if (heartsToNextLevel(destacado->level, destacado->heartsTotal,
		                      &cur, &tot)) {
			char bar[40];
			barra_str(bar, sizeof bar, cur, tot, 10);
			patc(TOP, 10, "%s %d/%d", bar, cur, tot);
		}
		patc(TOP, 12, "%ld días juntos", (long)daysTogether(destacado, hoy));
		if (isAtRisk(destacado, hoy)) {
			patc(TOP, 15, "! ¡Necesita atención!");
			patc(TOP, 16, "se va en %ld días",
			     (long)daysUntilLeaving(daysInactive(destacado, hoy)));
		}
	} else {
		wrap(TOP, 9, 3, 26,
		     "Tus relaciones aparecen aquí. Empieza creando una.");
	}
	uint16_t lider = rankingLeader(G);
	if (lider) {
		Character *lc = findCharacter(G, lider);
		patc(TOP, 19, "Va ganando la semana: %s",
		     fitname(lc->name, 10, false, nom, sizeof nom));
	}
	if (!g_fatOk) patc(TOP, 22, "[sin microSD: no se guarda]");

	// ---- abajo: acción
	zreset();
	for (int i = 0; i < MAX_CHARACTERS; i++) {
		int row = i * 3;
		if (slot[i]) {
			char nm[40];
			fitname(slot[i]->name, 14, true, nm, sizeof nm);
			pat(BOT, row, 0, "%d %s Lv%d", i + 1, nm, slot[i]->level);
			pat(BOT, row, 25, "[+M]");
			int16_t cur, tot;
			char bar[40];
			if (heartsToNextLevel(slot[i]->level, slot[i]->heartsTotal,
			                      &cur, &tot)) {
				barra_str(bar, sizeof bar, cur, tot, 8);
				pat(BOT, row + 1, 2, "%s %d/%d%s", bar, cur, tot,
				    isAtRisk(slot[i], hoy) ? " !" : "");
			}
			zadd(1 + i, row, 0, 2, 24);          // abrir perfil
			zadd(4 + i, row, 24, 2, 8);          // + misión
		} else {
			pat(BOT, row, 0, "%d [ + Crear personaje ]", i + 1);
			zadd(1 + i, row, 0, 2, 32);
		}
	}

	uint16_t ids[MAX_MISSIONS];
	int n = pendientes_ordenadas(ids, MAX_MISSIONS, hoy);
	*totalPages = (n + 3) / 4;
	if (*totalPages == 0) *totalPages = 1;
	if (n > 0)
		pat(BOT, 9, 0, "MISIONES PENDIENTES (%d)", n);
	else if (destacado)
		pat(BOT, 9, 0, "Sin misiones pendientes");
	for (int k = 0; k < 4; k++) {
		int idx = page * 4 + k;
		if (idx >= n) break;
		Mission *m = findMission(G, ids[idx]);
		Character *mc = findCharacter(G, m->characterId);
		char mn[40], cn[40], fr[20];
		fitname(m->name, 11, true, mn, sizeof mn);  // 11: la fila cierra en 32
		fitname(mc ? mc->name : "?", 5, true, cn, sizeof cn);
		fecha_rel(m->deadline, hoy, fr, sizeof fr);
		int row = 10 + k * 2;
		pat(BOT, row, 0, "%s %s %s %c %s",
		    m->deadline < hoy ? "!" : "·", mn, cn,
		    "FMD"[m->difficulty], fr);
		zadd(20 + idx, row, 0, 2, 32);
	}
	if (*totalPages > 1) {
		pat(BOT, 18, 11, "[ v pag %d/%d v ]", page + 1, *totalPages);
		zadd(8, 18, 8, 2, 18);
	}
	zbtn(7, 20, 0, 17, 3, "RANKING (X)");
	if (g_toast[0]) {
		pat(BOT, 23, 1, "%s", g_toast);
		g_toast[0] = 0;
	}
}

static void home_loop(void) {
	int page = 0;
	while (1) {
		GameDate hoy = today();
		int totalPages = 1;
		home_draw(hoy, page, &totalPages);
		if (page >= totalPages) { page = 0; continue; }
		int v = zwait(KEY_X);
		if (v == INP_X || v == 7) {
			scr_ranking();
		} else if (v == 8) {
			page = (page + 1) % totalPages;
		} else if (v >= 1 && v <= 3) {
			// slot: perfil si está ocupado, creador si está libre
			Character *c = 0;
			for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
				if (G->characters[i].id &&
				    G->characters[i].status == CHAR_ACTIVE &&
				    G->characters[i].slotNumber == v)
					c = &G->characters[i];
			if (c) scr_perfil(c->id);
			else if (firstFreeSlot(G)) scr_creador(false);
			else aviso("Habitaciones", "Las 3 habitaciones están ocupadas.",
			           "Entendido");
		} else if (v >= 4 && v <= 6) {
			Character *c = 0;
			for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
				if (G->characters[i].id &&
				    G->characters[i].status == CHAR_ACTIVE &&
				    G->characters[i].slotNumber == v - 3)
					c = &G->characters[i];
			if (c) scr_crear_mision(c->id);
		} else if (v >= 20) {
			uint16_t ids[MAX_MISSIONS];
			int n = pendientes_ordenadas(ids, MAX_MISSIONS, hoy);
			int idx = v - 20;
			if (idx < n) scr_mision(ids[idx]);
		}
	}
}

// ---------------------------------------------------------------- arranque

static void startup_queue(const StartupScenes *sc, const WeekCloseResult *wk) {
	// Escenas de nivel/boda no confirmadas (apagón entre el save de la
	// mutación y el CONTINUAR de la escena). Bits 1..2 = nivel, bit 3 = boda.
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &G->characters[i];
		if (!c->id) continue;
		if (c->status == CHAR_ACTIVE) {
			// tras bajar de nivel (GAP), re-alcanzarlo vuelve a dar escena
			c->milestonesShown &= (uint8_t)((1u << (c->level + 1)) - 1u);
			for (int lvl = 1; lvl <= c->level; lvl++)
				if (!(c->milestonesShown & (1u << lvl))) {
					CompleteResult r = { 0, true, (uint8_t)lvl, false };
					esc_nivel(c, &r);
					c->milestonesShown |= (uint8_t)(1u << lvl);
					guardar();
				}
		} else if (c->status == CHAR_HAPPY_ENDING &&
		           !(c->milestonesShown & 0x08)) {
			esc_boda(c);
			c->milestonesShown |= 0x08;
			guardar();
		}
	}
	// Orden de FASE3-INTEGRACION §3.1: cierre de semana, luego escenas.
	if (wk->closed && wk->winnerCharacterId)
		esc_resultado_semana(wk->winnerCharacterId);
	for (int i = 0; i < sc->count; i++) {
		const StartupScene *e = &sc->items[i];
		Character *c = findCharacter(G, e->characterId);
		if (!c) continue;
		if (e->kind == SCENE_ABANDONMENT) {
			esc_abandono(c, c->status == CHAR_ABANDONED);
			acknowledgeAbandonmentScene(G, c->id);
		} else {
			Mission *m = findMission(G, e->missionId);
			if (m) {
				int16_t pen = (int16_t)(m->heartsAwarded < 0
				                        ? -m->heartsAwarded : 0);
				esc_cancelacion(c, m->name, m->difficulty, pen,
				                m->status == MISSION_FAILED);
			}
			acknowledgeCancellationScene(G, c->id);
		}
		guardar();
	}
}

void ui_run(Save2 *sv, bool fatOk, Save2LoadResult loadResult,
            PrintConsole *top, PrintConsole *bottom) {
	TOP = top;
	BOT = bottom;
	SV = sv;
	G = &sv->state;
	g_fatOk = fatOk;
	g_toast[0] = 0;

	GameDate rtc = rtc_today();
	if (loadResult == SAVE2_BAD || loadResult == SAVE2_OLD_VERSION) {
		gameInit(G, rtc);
		aviso("Aviso",
		      "El archivo de guardado está dañado o es de una versión "
		      "vieja. Se archivó una copia en _hds/game.bad y el juego "
		      "empieza de cero.",
		      "Entendido");
		if (save2_quarantine_failed()) {
			// no se pudo apartar el original: no escribir encima de él
			aviso("Aviso",
			      "No se pudo apartar el archivo dañado. Para no "
			      "destruirlo, esta sesión no guardará nada.",
			      "Entendido");
			g_fatOk = false;
		}
	}
	if (loadResult != SAVE2_OK) {
		if (loadResult == SAVE2_ABSENT) gameInit(G, rtc);
		scr_inicio();
		while (!scr_creador(true)) {}
	} else {
		if (rtc < sv->lastSeenDay) {
			char f1[20], f2[20], msg[176];
			fecha_format_corta(rtc, f1, sizeof f1);
			fecha_format_corta(sv->lastSeenDay, f2, sizeof f2);
			snprintf(msg, sizeof msg,
			         "La fecha de la consola (%s) es anterior a la última "
			         "partida (%s). Revisa el reloj; mientras tanto el juego "
			         "sigue en la fecha guardada.", f1, f2);
			aviso("Reloj atrasado", msg, "Entendido");
		}
		GameDate hoy = today();
		StartupScenes scenes;
		WeekCloseResult week;
		buildStartup(G, hoy, &scenes, &week);
		guardar();  // persiste cierre de semana + abandonos antes de escenas
		startup_queue(&scenes, &week);
	}
	home_loop();
}
