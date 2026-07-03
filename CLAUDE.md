# CLAUDE.md — Habit Dating Sim DS

> Lee este archivo y PROGRESO.md al inicio de cada sesión. Actualiza PROGRESO.md al final.

---

## Qué es este proyecto

La versión Nintendo DS (ROM .nds) de Habit Dating Sim: un habit tracker disfrazado de dating sim. Cada hábito es un "personaje" con el que el usuario construye una relación completando misiones. Si falla (deadline vencido, 21 días sin actividad) la relación se deteriora. Si lo sostiene ~66 días llega a la boda y el hábito se consolida.

Esta NO es la app web. Es una reescritura en C para correr en hardware DS. La versión web (`habit-dating-sim-app`) sigue viva por separado y no se toca desde aquí. Lo único que se reusa de ella es la LÓGICA del juego (se porta a C) y los documentos de diseño.

**Un usuario. Sin servidor. Sin cuentas.** Los datos viven en un archivo de save en la microSD.

---

## Stack

- **C + devkitPro (devkitARM) + libnds** — código nativo DS
- **make + ndstool** — build a .nds
- **libfat** — guardado en la microSD (reemplaza localStorage)
- **RTC de la DS** vía `time()` — fechas reales para deadlines y el contador de 21 días
- **grit** — conversión de PNG a formatos DS (tiles, sprites, paletas)
- **melonDS** — emulador de desarrollo

---

## Fuentes de verdad del diseño

Viven en el Obsidian vault de Hector, en `projects/habit-dating-sim/`. Si el código contradice un doc, el doc gana (o se actualiza con decisión explícita de Hector en DECISIONS).

| Documento (en el vault) | Qué define |
|---|---|
| `nds-pivot/PLAN-NDS.md` | Plan maestro de fases de la versión DS |
| `nds-pivot/00-feasibility-director.md` | Estudio de factibilidad |
| `nds-pivot/01-toolchain.md` | Toolchain, build, hardware, save, RTC |
| `nds-pivot/02-port-logica.md` | Estrategia de port del motor a C |
| `nds-pivot/03-assets-ds.md` | Restricciones de hardware para arte |
| `nds-pivot/04-rankings-y-creador.md` | Spec de rankings y creador de personaje |
| `nds-pivot/05-escenas-responsivas.md` | Spec del motor de escenas compuestas |
| `design/mecanicas-detalle.md` | Mecánica exacta (corazones, niveles, penalizaciones, P4) |
| `DECISIONS.md` | Decisiones de producto. Tiene precedencia cuando contradice mecanicas-detalle |
| `testing/qa-report.md` | 42 casos de prueba (se reescriben como harness de C en Fase 2) |

Las órdenes de construcción por fase viven en `projects/habit-dating-sim/equipo/` (ej: `WORK-ORDER-FASE-0-NDS.md`).

---

## Reglas de trabajo

1. **Leer PROGRESO.md al inicio** de cada sesión para saber dónde quedó el trabajo.
2. **Tests primero en la mecánica.** Cuando llegue el port del motor (Fase 2), ninguna regla del juego termina sin test que la cubra. Los tests corren como harness de C en PC, no en la DS.
3. **El motor de juego es PURO.** No puede llamar a libnds, no puede leer el RTC ni el save directamente: recibe la fecha como parámetro (igual que en la versión web con `Date`). Así los tests son deterministas.
4. **Commit por milestone** con mensaje descriptivo en español. Nunca cerrar sesión con trabajo sin commitear.
5. **Actualizar PROGRESO.md al final** de cada sesión.
6. **Español en la UI y los docs, inglés en el código.**
7. **No agregar librerías** fuera de devkitPro/libnds sin anotarlo y justificarlo en PROGRESO.md.
8. **Hector no programa.** Explícale en términos de "qué se ve y qué hace en la consola". Sus pruebas en la DS real son el control de calidad.
9. **No tocar la versión web** (`habit-dating-sim-app`).

---

## Fases del plan (resumen, detalle en PLAN-NDS.md)

| Fase | Nombre | Estado |
|---|---|---|
| 0 | Setup (toolchain + repo + hello world .nds) | ⏳ En curso |
| 1 | Cimientos de riesgo (RTC + save libfat + fuente en español) | Pendiente |
| 2 | Port del motor a C (+ tests, P4, +4 campos de apariencia, ranking) | Pendiente |
| 3 | Pantallas, input táctil, creador de personaje, pantalla de ranking | Pendiente |
| 4 | Assets y motor de escenas compuestas en runtime | Pendiente |
| 5 | Pulido y release del .nds final | Pendiente |

---

## Decisiones clave ya tomadas (no reabrir sin Hector)

- **P4:** misión vencida = derecho de réplica (se completa tarde con recompensa reducida por multiplicador 75/50/25%), NO autofallo.
- **Creador de personaje:** identidad cosmética, 4 ejes por selección (base, paleta, accesorio, expresión). +4 campos `uint8` en el struct Character. Nunca afecta mecánica.
- **Rankings:** reality show semanal entre los habiters del usuario, métrica = corazones netos de la semana (reset lunes 00:00 anclado al RTC), #1 solo cosmético. Entra al MVP.
- **Escenas responsivas:** se componen en runtime (fondo + personaje custom + estado), no son PNGs estáticos.

---

## Modelo de datos clave (resumen, detalle en 02-port-logica.md)

```c
// Arrays fijos, sin malloc. IDs enteros. Tiempo como días desde epoch.
typedef struct {
  uint8_t  level;            // 0..3, INDEPENDIENTE de heartsTotal
  int16_t  heartsTotal;      // único contador, mínimo 0
  int32_t  lastCompletedDay; // GameDate (días desde epoch), o -1
  // apariencia (creador de personaje, solo cosmético):
  uint8_t  base, palette, accessory, expression;
  // ...
} Character;
// Umbrales de nivel: 20 / 60 / 140. Sube cuando una completación cruza el umbral.
// Baja 1 por abandono (21 días inactivo). heartsTotal NO cambia por abandono.
// Misión vencida: P4 (no autofallo).
```

---

## Comandos del repo

```bash
make            # compila a .nds
make clean      # limpia artefactos de build
```

(Los comandos de test del harness de C se definen al llegar a la Fase 2.)
