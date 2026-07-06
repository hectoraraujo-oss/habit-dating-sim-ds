// Guardado en microSD vía libfat. Escritura atómica (tmp + rename),
// con magic, versión y CRC32 para detectar corrupción.
#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stdint.h>

#define SAVE_MAGIC   0x31534448u // "HDS1"
#define SAVE_VERSION 1

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t reserved;
	uint32_t bootCount;    // veces que ha encendido el juego
	uint32_t testCounter;  // contador manual de la demo (botón A)
	uint32_t lastBootTime; // unix time del último boot
	uint32_t crc;          // CRC32 de todos los campos anteriores
} SaveData;

// Monta la FAT de la microSD. false = no hay SD/DLDI (la demo sigue sin save).
bool save_init(void);

// Lee y valida el save. false = no existe o está corrupto (out queda en ceros).
bool save_load(SaveData *out);

// Escritura atómica: save.tmp -> rename a save.dat. Calcula el CRC.
bool save_write(SaveData *data);

// CRC32 estándar (IEEE), expuesto para el harness de PC.
uint32_t save_crc32(const void *buf, unsigned int len);

#endif
