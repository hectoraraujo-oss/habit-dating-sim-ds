// Conversión civil <-> GameDate (días desde 1970-01-01), entera y pura.
// El RTC de la DS da hora local; la UI convierte con localtime() y estas
// funciones, así el motor nunca ve timezones ni DST.
#ifndef FECHA_H
#define FECHA_H

#include "engine.h"

GameDate fecha_from_civil(int y, int m, int d);            // m 1..12, d 1..31
void     fecha_to_civil(GameDate g, int *y, int *m, int *d);
int      fecha_weekday(GameDate g);                        // 0=domingo .. 6=sábado

// "lun 6 jul" en out (mínimo 16 bytes), en español.
void fecha_format_corta(GameDate g, char *out, unsigned n);

// Nombres en español para componer fechas largas en la UI.
const char *fecha_dia_nombre(int weekday);  // 0=domingo .. 6=sábado
const char *fecha_mes_nombre(int m);        // 1..12

#endif
