// Save v2: la partida completa (GameState) en la microSD.
// Contrato definido en FASE3-INTEGRACION.md (vault). Mismo patrón de Fase 1:
// magic, versión, CRC32 al final, escritura atómica tmp -> rename.
// Convive con /_hds/fase1.sav (demo de Fase 1, se ignora).
// Requiere save_init() antes de usarse.
#ifndef SAVE2_H
#define SAVE2_H

#include <stdbool.h>
#include <stdint.h>

#include "engine.h"

#define SAVE2_MAGIC   0x32534448u  // "HDS2" little-endian en disco
#define SAVE2_VERSION 2

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t stateSize;    // sizeof(GameState): guard contra drift de layout
	uint32_t saveCount;    // nº de escrituras (diagnóstico)
	GameDate lastSeenDay;  // último "hoy" persistido; guardia anti reloj-hacia-atrás
	GameState state;
	uint32_t crc;          // CRC32 de todos los bytes anteriores
} Save2;

typedef enum {
	SAVE2_OK = 0,       // cargado y válido
	SAVE2_ABSENT,       // no existe game.sav: primer arranque
	SAVE2_BAD,          // corrupto; quedó en cuarentena como game.bad
	SAVE2_OLD_VERSION,  // versión incompatible; también en cuarentena
} Save2LoadResult;

// Si no devuelve SAVE2_OK, *out queda en ceros: el caller debe gameInit().
// BAD y OLD_VERSION dejan el archivo original renombrado a game.bad (nunca
// se sobreescribe en silencio un save que no entendemos).
Save2LoadResult save2_load(Save2 *out);

// Rellena magic/version/stateSize/crc e incrementa saveCount. Atómico.
bool save2_write(Save2 *data);

// true si la última cuarentena (BAD/OLD_VERSION) no pudo renombrar el
// archivo: el caller debe jugar en modo sesión para no destruir el original.
bool save2_quarantine_failed(void);

#endif
