// UI de Fase 3: pantallas dual-screen en modo texto, input de stylus.
// Contratos: FASE3-PANTALLAS.md y FASE3-INTEGRACION.md (vault).
#ifndef UI_H
#define UI_H

#include <nds.h>
#include <stdbool.h>

#include "save2.h"

// Loop principal del juego. No regresa.
// loadResult decide el flujo de entrada (primer arranque / corrupto / normal).
void ui_run(Save2 *sv, bool fatOk, Save2LoadResult loadResult,
            PrintConsole *top, PrintConsole *bottom);

#endif
