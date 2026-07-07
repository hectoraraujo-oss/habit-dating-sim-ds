#include "fecha.h"

#include <stdio.h>

// Algoritmos de Howard Hinnant (days_from_civil / civil_from_days),
// aritmética entera exacta para todo el rango del calendario gregoriano.

GameDate fecha_from_civil(int y, int m, int d) {
	y -= m <= 2;
	int era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);
	unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)d - 1u;
	unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
	return (GameDate)(era * 146097 + (int)doe - 719468);
}

void fecha_to_civil(GameDate g, int *y, int *m, int *d) {
	int z = g + 719468;
	int era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
	int yy = (int)yoe + era * 400;
	unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
	unsigned mp = (5u * doy + 2u) / 153u;
	*d = (int)(doy - (153u * mp + 2u) / 5u + 1u);
	*m = (int)(mp < 10 ? mp + 3 : mp - 9);
	*y = yy + (*m <= 2);
}

int fecha_weekday(GameDate g) {
	// 1970-01-01 fue jueves (4).
	return (int)(((g % 7) + 7 + 4) % 7);
}

static const char *DIAS_CORTOS[7]  = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
static const char *MESES_CORTOS[12] = { "ene", "feb", "mar", "abr", "may", "jun",
                                        "jul", "ago", "sep", "oct", "nov", "dic" };

void fecha_format_corta(GameDate g, char *out, unsigned n) {
	int y, m, d;
	fecha_to_civil(g, &y, &m, &d);
	snprintf(out, n, "%s %d %s", DIAS_CORTOS[fecha_weekday(g)], d, MESES_CORTOS[m - 1]);
}

static const char *DIAS_LARGOS[7] = {
	"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"
};
static const char *MESES_LARGOS[12] = {
	"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio",
	"agosto", "septiembre", "octubre", "noviembre", "diciembre"
};

const char *fecha_dia_nombre(int weekday) {
	return DIAS_LARGOS[((weekday % 7) + 7) % 7];
}

const char *fecha_mes_nombre(int m) {
	return MESES_LARGOS[(m >= 1 && m <= 12) ? m - 1 : 0];
}
