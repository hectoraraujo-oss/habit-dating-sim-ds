// Harness de PC del módulo de fechas. Compilar y correr:
//   gcc -Wall -Wextra -o fecha_test.exe test/fecha_test.c source/fecha.c
//   ./fecha_test.exe
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../source/fecha.h"

int main(void) {
	// Anclas conocidas.
	assert(fecha_from_civil(1970, 1, 1) == 0);
	assert(fecha_from_civil(2000, 3, 1) == 11017);  // valor de referencia de Hinnant
	assert(fecha_weekday(0) == 4);                  // 1970-01-01 fue jueves

	// 2026-07-06 es lunes (weekday 1).
	GameDate hoy = fecha_from_civil(2026, 7, 6);
	assert(fecha_weekday(hoy) == 1);

	// Roundtrip diario por ~30 años alrededor de hoy (cubre bisiestos y
	// fines de siglo no-bisiestos quedan fuera de rango de uso real).
	int y, m, d;
	for (GameDate g = fecha_from_civil(2010, 1, 1); g <= fecha_from_civil(2040, 12, 31); g++) {
		fecha_to_civil(g, &y, &m, &d);
		assert(fecha_from_civil(y, m, d) == g);
		assert(m >= 1 && m <= 12 && d >= 1 && d <= 31);
	}

	// Bisiesto: 2028-02-29 existe y es consecutivo a 2028-02-28.
	assert(fecha_from_civil(2028, 2, 29) == fecha_from_civil(2028, 2, 28) + 1);
	assert(fecha_from_civil(2028, 3, 1) == fecha_from_civil(2028, 2, 29) + 1);

	// Formato corto en español.
	char buf[16];
	fecha_format_corta(hoy, buf, sizeof buf);
	assert(strcmp(buf, "lun 6 jul") == 0);

	puts("FECHA TESTS OK");
	return 0;
}
