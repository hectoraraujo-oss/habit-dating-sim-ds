# PROGRESO — Habit Dating Sim DS

> Log de avance por sesión. Append-only. Más reciente arriba.
> Fuente de verdad del avance fino. Se lee al abrir sesión y se actualiza al cerrar.

---

## [2026-07-06] (5) Fase 3 — Pantallas y UI: CONSTRUIDA, QA pasada, falta el gate de consola

**Estado:** el loop completo es jugable con stylus. La ROM (312 KB) compila sin warnings, arranca en melonDS a 60 fps y las 4 suites de PC están en verde (motor 211 checks, save2, fecha, host). Falta el gate de Hector en su consola.

### Qué se construyó
- **`source/save2.{h,c}` — save v2:** la partida completa (`GameState`, 2092 bytes) con magic HDS2, versión, `stateSize` como guard de layout (+ `_Static_assert`s), CRC32, escritura atómica, `lastSeenDay` (guardia anti reloj-hacia-atrás) y cuarentena de saves corruptos en `game.bad` (nunca se pisa un save que no se entiende; si la cuarentena falla, la sesión no escribe). Archivo: `/_hds/game.sav`.
- **`source/fecha.{h,c}`:** civil <-> GameDate (algoritmo de Hinnant, entero puro), weekday, formatos en español. El RTC se lee vía `localtime` y se convierte con esto; prohibido `time()/86400`.
- **`source/ui.{h,c}` (~950 líneas) — las 10 pantallas de FASE3-PANTALLAS.md:** V0 inicio, V1 Home (3 slots + lista de pendientes paginada + destacado arriba), V2 Perfil (historial paginado, banner de riesgo), V3 Crear misión (dificultad con copy literal, deadline hoy..hoy+14, P5), V4 Completar con variante vencida P4 (dos botones, preview con `calcHeartsEarned`), V5 Nivel + variante Boda (D2), V6 Abandono (salida definitiva o solo bajada de nivel; copy alterno si nunca completó nada), V7 Cancelación/pérdida (con aviso extra en 0 ♥), V8 Creador de 4 ejes (base/color/accesorio/expresión, accesorio 0 = ninguno), V9 Ranking semanal (podio solo con líder real, tabla con netos, desempate espejo del motor). Teclado táctil propio (A-Z + Ñ + espacio, borrado UTF-8-consciente). Cola de startup: escenas de nivel/boda re-hidratadas vía `milestonesShown` → resultado de semana → abandonos/cancelaciones del motor, con acknowledge + save por escena.
- **Política de guardado:** una escritura tras cada mutación `ENGINE_OK` (y tras cada acknowledge de escena). `today()` se re-muestrea en cada acción, clampeado a `lastSeenDay`.

### QA adversarial (FASE3-QA-AUDIT.md en el vault): APTO CON FIXES → fixes aplicados
- **A1 (fix aplicado):** `es_convert` no decodificaba UTF-8 de 3 bytes; ♥ ░ √ · salían como `?` en toda la UI. Ahora hay rama de 3 bytes + asserts en host_test.
- **A2 (fix aplicado):** V3/V4 usaban la fecha de entrada a la pantalla; ahora cada mutación re-muestrea `today()` (consola dormida días ya no rompe P4 ni el ancla de inactividad).
- **M1 (fix aplicado):** apagón entre el save y el CONTINUAR de una escena de nivel/boda ya no se la traga: se re-hidrata al arrancar con bits en `milestonesShown` (bits 1-2 nivel, 3 boda; al bajar de nivel se limpian los bits superiores para que re-alcanzar dé escena).
- **B1, podio, copy de abandono, desborde de fila en Home, bound de ranking_orden, es_convert(n=0) (fixes aplicados).**

### Decisiones de constructor tomadas en esta fase (Hector: bendecir o vetar en el gate)
1. **Input solo stylus + B (atrás) + X (ranking desde Home).** El espejo D-pad/A/L/R de la spec quedó fuera (M2 de la QA). En consola el stylus siempre está; si en el gate se siente incómodo, se agrega.
2. **Copy de boda propuesto** ("Lo logramos. Esto ya no es una promesa: es parte de quién eres. Sí, acepto.") — no había copy en el vault (A1 de FASE3-PANTALLAS).
3. **Teclado sin acentos/minúsculas** y nombres a máx 20 celdas visibles.
4. **Reagendar misión sin UI** (el motor lo soporta; la advertencia de V3 queda como aviso general). Si se quiere, es una pantalla más.
5. **Semana cerrada sin ganador no genera pantalla** al arrancar (solo se ve en V9).
6. **Accesorio índice 0 = ninguno** (cerrado en FASE3-INTEGRACION §4; Fase 4 indexa assets 1..4).

### Deuda anotada (no bloquea el gate)
- Contador de canceladas en Perfil cuenta solo lo vivo en `Mission[16]` (se deflacta con el reciclaje); arreglarlo pide un contador persistente → lote con el próximo `SAVE2_VERSION`+1.
- Menciones narrativas de eliminado/graduado en el ranking (V9) no construidas.
- Toast puede aparecer tarde si se crea misión desde Perfil y se vuelve a Home mucho después.
- Registro de boda esperando escena podría reciclarse si los 8 records se llenan (ventana mínima).

### Cómo correr todo
```
# ROM
C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/Dirhector/Desktop/habit-dating-sim-ds && make"
# Suites de PC (ver CLAUDE.md del repo para los 4 comandos)
```

### GATE DE HECTOR (cierra la fase)
Con la ROM nueva en la SD (reemplazar la vieja): 1) primer arranque → pantalla de inicio → crear tu primer habiter (4 ejes + nombre con el teclado táctil); 2) crearle una misión y completarla (deben subir corazones y guardarse); 3) apagar y prender → debe recordar todo; 4) probar el ranking (botón X), cancelar una misión (escena de decepción) y navegar con B. Si algo se siente raro con el stylus, es feedback directo para las decisiones 1-5 de arriba.

### Próxima fase
Fase 4 (assets y motor de escenas compuestas): los 4 ejes del creador ya persisten en el save; el descriptor está cerrado (base 0-3, paleta 0-3, accesorio 0=ninguno/1-4, expresión 0-2, default {0,0,0,0} con arte garantizado).

---

## [2026-07-06] (4) Supuestos de Fase 2 CONFIRMADOS por Hector ✅ — arranca Fase 3

Hector confirmó los 4 supuestos anotados abajo (semana lunes, netos nominales, cierre de semana antes de abandono, GAP nivel/corazones heredado de la web). Quedan fijados como comportamiento oficial; los tests que los cubren son ahora spec, no supuesto. Arranca Fase 3 (pantallas y UI).

---

## [2026-07-06] (3) Fase 2 — Motor portado a C: GATE CUMPLIDO ✅ (211 checks en verde)

**Estado:** la lógica completa del juego vive en `source/engine.{h,c}` (pura: sin libnds, sin RTC, sin save; la fecha entra como `GameDate` = días enteros desde epoch). La spec ejecutable es `test/engine_test.c`: **211 checks en verde a la primera**, cubriendo los 42 TC de qa-report, las suites P4/P5 del motor web, hearts/consultas/startup, y tests nuevos del ranking semanal P12. La ROM sigue compilando con el motor incluido (falta cablearlo a UI y save: eso es Fase 3).

### Proceso
Workflow de 4 lectores paralelos destiló las fuentes de verdad (mecanicas-detalle + DECISIONS, qa-report, motor web vivo función por función, y 02-port-logica). El motor web vivo (`habit-dating-sim-app/src/game/`) fue la referencia de comportamiento; DECISIONS mandó en conflictos (P4 sobre D3, M-2026-06-11-A/B).

### Modelo portado (resumen)
- Structs planos sin malloc: `Character[8]` (3 activos + históricos), `Mission[16]` (9 pendientes máx + historial reciente con reciclaje del cerrado más viejo), `HappyEnding[16]`. IDs `uint16` autoincrementales, 0 = vacío/null.
- Mecánica: corazones 5/10/18, umbrales 20/60/140 (nivel sube de a 1 por misión, TC-012), penalizaciones 3/5/8 con clamp a 0, nivel nunca baja por penalización, abandono acumulable con ancla `inactivitySince` que nunca para (21 días por nivel; en 0 se va y libera slot), P4 réplica con multiplicadores 100/75/50/25 (ceil entero sin floats, piso 25% sin ventana), P5 deadline hoy válido, boda al cruzar 140 (cancela pendientes sin penalización, registra HappyEnding).
- P12: `weeklyHearts` (netos, puede ser negativo), `weeksWon`, `weekAnchor` (lunes), cierre on-load con desempate determinista (más recientemente activo → creado antes → slot menor), #1 solo con neto > 0. +4 campos uint8 de apariencia (cosméticos).
- `buildStartup(today)`: cierre de semana → checkAbandonment → cola de escenas (nuevas + re-hidratadas, flag huérfano se limpia).

### Supuestos anotados para Hector (no bloquean, los tests los fijan; se cambian con una constante)
1. **Semana arranca lunes** (supuesto del spec de rankings; ¿lunes o domingo?).
2. **Netos semanales = deltas nominales**: la penalización resta completa al ranking aunque `heartsTotal` clampee en 0 (G6 del spec).
3. **Cierre de semana ANTES del check de abandono** al abrir (el podio se congela con los estados de la última sesión).
4. **GAP heredado del motor web**: al bajar de nivel por abandono, `heartsTotal` no se recorta; la siguiente misión completada puede restaurar el nivel de inmediato. Portado literal (así se comporta la web hoy).

### Cómo correr los tests del motor
```
C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/Dirhector/Desktop/habit-dating-sim-ds && gcc -Wall -Wextra -o engine_test.exe test/engine_test.c source/engine.c && ./engine_test.exe"
```

### Próxima fase
Fase 3 (pantallas y UI): layout dual-screen, input táctil, creador de personaje (los 4 ejes ya existen en el struct), pantalla de ranking, y cablear motor + save de Fase 1 (serializar `GameState` con el patrón magic/versión/CRC ya probado).

---

## [2026-07-06] (2) GATE DE CONSOLA SUPERADO — Fases 0 y 1 CERRADAS ✅

Hector probó la ROM en su consola real (3DS + TWiLight Menu++): acentos correctos, fecha real correcta, y los contadores ("Encendidos" y "Corazones") recuerdan entre apagones. Con eso quedan validados en hardware: cadena de build, RTC, save en microSD vía nds-bootstrap y fuente en español. Arranca Fase 2 (port del motor a C).

---

## [2026-07-06] Fase 1 — Cimientos de riesgo: CONSTRUIDA y verificada en emulador

**Estado:** las 3 piezas de riesgo funcionan en melonDS. Falta el gate de Hector en consola real (sin pila hoy), que valida Fase 0 y Fase 1 juntas con esta misma ROM.

### Hecho hoy
- [x] **Fuente en español:** hallazgo clave: la fuente default de libnds ya es CP437 y trae á é í ó ú ñ Ñ ü ¿ ¡ É dibujados. No hizo falta fuente custom, solo `source/es.c`: conversor UTF-8 → CP437 (`es_convert` + `es_printf`). Á Í Ó Ú Ü no existen en CP437 y caen a A I O U U (ponytail comment en el código; si un copy las necesita, se parchan 5 glifos en RAM).
- [x] **RTC:** `time(NULL)` + `localtime()` funcionan; fecha mostrada en español ("lunes 6 de julio de 2026") con reloj vivo por segundo.
- [x] **Save en microSD:** `source/save.c` con libfat. Archivo `/_hds/fase1.sav` (24 bytes): magic HDS1 + versión + CRC32 + escritura atómica (tmp → rename). Verificado en melonDS con SD virtual (DLDI folder-sync): bootCount y testCounter sobrevivieron 2 reinicios, CRC validado, y el hex del archivo se inspeccionó byte por byte.
- [x] **Harness de PC:** `test/host_test.c` compila `es.c` y `save.c` con gcc nativo. Cubre mapeo de acentos, fallbacks, truncado seguro, roundtrip del save y detección de corrupción (byte volteado → CRC inválido → carga rechazada). `HOST TESTS OK`.
- [x] Demo interactiva: botón A suma "corazones de prueba" y guarda al instante.
- [ ] **GATE (Fase 0 + Fase 1):** Hector prende su consola (hoy sin pila) y verifica: texto en español con acentos, fecha real correcta, y que "Encendidos" y "Corazones" recuerdan entre apagones. ROM ya copiada a su SD (`D:\Juegos_NDS\habit-dating-sim-ds.nds`, hash verificado) y al Desktop.

### Cómo correr los tests de PC
```
C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/Dirhector/Desktop/habit-dating-sim-ds && gcc -Wall -Wextra -o host_test.exe test/host_test.c source/es.c source/save.c && ./host_test.exe"
```

### Notas técnicas
- melonDS quedó configurado con SD virtual (DLDI folder-sync) y la tecla A del teclado mapeada al botón A (keycode Qt 65), en `%LOCALAPPDATA%\Programs\melonDS\melonDS.toml`. Sin DLDI, la demo muestra "microSD: NO disponible" y sigue corriendo (no crashea).
- Evidencia visual en el vault: `projects/habit-dating-sim/logs/fase1-boot{1,2-corazones,3-persistencia}-2026-07-06.png`.

### Próxima fase
Fase 2 (port del motor a C): structs `Character`/`Mission`/`GameState`, mecánica completa (corazones 5/10/18, umbrales 20/60/140, P4 réplica, abandono 21 días, ranking semanal) + los 42 tests como harness de PC (el harness de hoy es la semilla).

---

## [2026-07-03] Fase 0 — Setup: build funcionando, falta el gate de consola

**Estado:** toolchain instalado, repo creado, hello world compilado y verificado en melonDS. Solo falta que Hector cargue el `.nds` en su consola real para cerrar la fase.

### Hecho hoy
- [x] Bloque 0.1: toolchain instalado. Vía scriptable: MSYS2 base en `C:\msys64` + repos oficiales de devkitPro en pacman + grupo `nds-dev` completo (devkitARM, libnds, libfat, ndstool, grit, ejemplos) + `make`. melonDS 1.1 portable en `%LOCALAPPDATA%\Programs\melonDS`.
- [x] Bloque 0.2: repo `habit-dating-sim-ds` con scaffold del template arm9 de libnds, `.gitignore`, `CLAUDE.md` y `PROGRESO.md` en la raíz.
- [x] Bloque 0.3: `source/main.c` imprime en ambas pantallas; `make` produce `habit-dating-sim-ds.nds` (173 KB) sin errores.
- [x] Bloque 0.4a: verificado en melonDS. Pantalla superior: "Habit Dating Sim / Nintendo DS". Inferior: "Fase 0 OK / La cadena de build vive." Corre a 60 fps.
- [ ] Bloque 0.4b (GATE DE CIERRE): Hector carga el `.nds` en su consola y confirma que enciende. Copia del archivo en el Desktop: `habit-dating-sim-ds.nds`.
  - Primer intento (2026-07-03): 3DS + forwarder → nds-bootstrap dio error. El archivo se verificó sano en PC (ndstool: header/logo/banner/secure area CRC OK, hashes idénticos entre repo y Desktop, corre en melonDS). Diagnóstico apunta al lado consola/forwarder.
  - Diagnóstico con la SD en la PC (2026-07-03): copia en SD byte-idéntica al original (SHA256). Causa raíz probable: el forwarder NTR lanza la ROM con `nds-bootstrap-release` (bootstrap de juegos comerciales, que la trata como cartucho y la parchea) en vez de `nds-bootstrap-hb-release` (homebrew). Evidencia: el `.sav` sí se creó (el bootstrap corrió y murió después), los juegos comerciales del mismo setup funcionan, y no había TWiLight Menu++ en la SD. El bootstrap además estaba viejo (v0.60.0).
  - Fix aplicado en la SD desde la PC: nds-bootstrap actualizado a v2.16.0 + TWiLight Menu++ v27.24.1 copiado (`_nds/TWiLightMenu`, `roms/`, CIA en `sd:/cias/`). Backup de los bootstraps previos en `sd:/_nds-backup-20260703/`. Falta que Hector instale la CIA con FBI y lance el juego desde TWiLight Menu++ (detecta homebrew y usa el bootstrap hb automáticamente).

### Cómo compilar en esta máquina
```
C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/Dirhector/Desktop/habit-dating-sim-ds && make"
```
(El entorno devkitARM se carga solo desde `/etc/profile.d/devkit-env.sh` en el login shell de MSYS2.)

### Notas
- devkitPro se instaló por la vía pacman-en-MSYS2 (lo mismo que arma el instalador gráfico oficial, pero scriptable y sin admin). Documentado en el wiki oficial de devkitPro.
- El Makefile es el template arm9 oficial con `GAME_TITLE` para el banner de la ROM. `TARGET` sale del nombre de la carpeta.

---

## [2026-06-29] Fase 0 — Setup ⏳ EN CURSO

**Estado:** repo recién creado. Arrancando la Fase 0 del plan DS.

**Orden de trabajo de esta fase:** `WORK-ORDER-FASE-0-NDS.md` (en el vault, `projects/habit-dating-sim/equipo/`).

### Objetivo de la fase
Probar la cadena de build y que un `.nds` arranque en la consola de Hector. NO se toca lógica de juego.

### Pendiente
- [ ] Bloque 0.1: instalar devkitPro + libnds (grupo `nds-dev`) + melonDS.
- [ ] Bloque 0.2: scaffold del repo desde el template de libnds, `.gitignore`, este `CLAUDE.md` y `PROGRESO.md` en la raíz.
- [ ] Bloque 0.3: hello world en `source/main.c` que imprima texto y compile con `make` a `.nds`.
- [ ] Bloque 0.4: verificar en melonDS y que Hector lo cargue en su consola (gate de cierre).

### Notas
- Acentos en español: NO en Fase 0. La fuente custom con ñ/á/é/í/ó/ú es trabajo de Fase 1.
- Si el setup de devkitPro en Windows da problemas, BlocksDS es la alternativa (ver `01-toolchain.md`), pero no cambiar de toolchain sin avisar al director del proyecto.

### Próxima fase
Fase 1 (cimientos de riesgo): RTC con `time()`, save con libfat, y la fuente bitmap en español. Es la fase más importante porque ataca lo que puede hundir el proyecto antes de invertir en lógica y arte.

---

## Contexto del proyecto

Reescritura a Nintendo DS de Habit Dating Sim. La versión web (`habit-dating-sim-app`) sigue viva por separado. El plan completo, las decisiones y los specs viven en el Obsidian vault de Hector en `projects/habit-dating-sim/` (ver `CLAUDE.md` de este repo para el mapa de documentos). Director del proyecto: Claude en el vault. Constructor: Claude Code en esta máquina. Product owner: Hector (no programa, prueba en la consola real).
