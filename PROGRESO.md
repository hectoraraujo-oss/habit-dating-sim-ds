# PROGRESO — Habit Dating Sim DS

> Log de avance por sesión. Append-only. Más reciente arriba.
> Fuente de verdad del avance fino. Se lee al abrir sesión y se actualiza al cerrar.

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
