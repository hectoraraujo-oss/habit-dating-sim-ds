// Spec ejecutable del motor (Fase 2). Porta:
// - los 42 casos de testing/qa-report.md (TC-001..TC-042)
// - las suites P4 (derecho de réplica) y P5 (deadline hoy) del motor web
// - los tests de consultas/hearts/startup del motor web
// - tests nuevos del ranking semanal (P12)
// Corre en PC con gcc nativo. Fechas inyectadas: GameDate = días enteros.
#include <stdio.h>
#include <string.h>

#include "../source/engine.h"

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { \
	printf("FALLO linea %d: %s\n", __LINE__, #cond); failures++; } } while (0)

// TODAY = 20000 (viernes). MONDAY = 20003 (lunes).
static const GameDate TODAY = 20000;
static const GameDate MONDAY = 20003;

static GameState S;

static void reset(void) { gameInit(&S, TODAY); }

static uint16_t mkChar(const char *name, GameDate created, uint8_t level,
                       int16_t hearts, GameDate inactivity) {
	uint16_t id = 0;
	createCharacter(&S, name, created, &id);
	Character *c = findCharacter(&S, id);
	c->level = level;
	c->heartsTotal = hearts;
	c->inactivitySince = inactivity;
	return id;
}

// Crea la misión "en el pasado" si el deadline ya venció (today = deadline).
static uint16_t mkMission(uint16_t cid, Difficulty d, GameDate deadline) {
	uint16_t id = 0;
	createMission(&S, cid, "mision", d, deadline, deadline, &id);
	return id;
}

// ---------- Área 1: corazones ----------
static void area1(void) {
	CompleteResult r;
	int16_t pen;

	// TC-001/002/003: recompensa por dificultad
	reset();
	uint16_t c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	CHECK(completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), TODAY, &r) == ENGINE_OK);
	CHECK(r.heartsEarned == 5 && !r.leveledUp);
	CHECK(findCharacter(&S, c1)->heartsTotal == 5);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY + 7), TODAY, &r);
	CHECK(r.heartsEarned == 10);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	completeMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), TODAY, &r);
	CHECK(r.heartsEarned == 18);

	// TC-004/005/006: penalización por cancelar
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	CHECK(cancelMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), &pen) == ENGINE_OK);
	CHECK(pen == 3 && findCharacter(&S, c1)->heartsTotal == 7);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY + 7), &pen);
	CHECK(pen == 5 && findCharacter(&S, c1)->heartsTotal == 5);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 15, TODAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), &pen);
	CHECK(pen == 8 && findCharacter(&S, c1)->heartsTotal == 7);

	// TC-007/008: la penalización no baja el nivel
	reset(); c1 = mkChar("A", TODAY - 30, 1, 50, TODAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), &pen);
	CHECK(findCharacter(&S, c1)->heartsTotal == 42 && findCharacter(&S, c1)->level == 1);
	reset(); c1 = mkChar("A", TODAY - 30, 1, 25, TODAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), &pen);
	CHECK(findCharacter(&S, c1)->heartsTotal == 17 && findCharacter(&S, c1)->level == 1);
}

// ---------- Área 2: progresión ----------
static void area2(void) {
	CompleteResult r;

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 15, TODAY - 1); // TC-009
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY + 7), TODAY, &r);
	CHECK(r.leveledUp && r.newLevel == 1 && findCharacter(&S, c1)->heartsTotal == 25);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 14, TODAY - 1);          // TC-010
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), TODAY, &r);
	CHECK(!r.leveledUp && findCharacter(&S, c1)->level == 0 && findCharacter(&S, c1)->heartsTotal == 19);

	reset(); c1 = mkChar("A", TODAY - 30, 1, 55, TODAY - 1);          // TC-011
	completeMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), TODAY, &r);
	CHECK(r.leveledUp && r.newLevel == 2 && findCharacter(&S, c1)->heartsTotal == 73);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 18, TODAY - 1);          // TC-012: solo 1 nivel
	completeMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), TODAY, &r);
	CHECK(findCharacter(&S, c1)->heartsTotal == 36 && findCharacter(&S, c1)->level == 1);

	reset(); c1 = mkChar("A", TODAY - 30, 2, 135, TODAY - 1);         // TC-013: boda
	completeMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), TODAY, &r);
	CHECK(r.leveledUp && r.newLevel == 3 && r.wedding);
}

// ---------- Área 3: escenas ----------
static void area3(void) {
	CompleteResult r;
	int16_t pen;
	AbandonmentEvents ev;

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1); // TC-014
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), TODAY, &r);
	CHECK(!r.leveledUp && findCharacter(&S, c1)->heartsTotal == 15);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);          // TC-015
	uint16_t m = mkMission(c1, DIFF_EASY, TODAY + 7);
	cancelMission(&S, m, &pen);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 1);
	CHECK(findCharacter(&S, c1)->heartsTotal == 7);
	CHECK(findMission(&S, m)->status == MISSION_CANCELLED);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);          // TC-016
	m = mkMission(c1, DIFF_EASY, TODAY - 1);
	CHECK(acceptMissionLoss(&S, m, &pen) == ENGINE_OK && pen == 3);
	CHECK(findMission(&S, m)->status == MISSION_FAILED);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 1);
	CHECK(findCharacter(&S, c1)->heartsTotal == 7);

	reset(); c1 = mkChar("A", TODAY - 30, 1, 25, TODAY - 21);         // TC-017: justo 21 días
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 1);
	CHECK(findCharacter(&S, c1)->pendingAbandonmentScene == 1);
	CHECK(findCharacter(&S, c1)->level == 0);
	CHECK(findCharacter(&S, c1)->heartsTotal == 25);

	reset(); c1 = mkChar("A", TODAY - 30, 1, 25, TODAY - 20);         // TC-018: 20 días
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 0);
	CHECK(findCharacter(&S, c1)->pendingAbandonmentScene == 0);
	CHECK(findCharacter(&S, c1)->level == 1);
	CHECK(findCharacter(&S, c1)->status == CHAR_ACTIVE);
}

// ---------- Área 4: abandono ----------
static void area4(void) {
	AbandonmentEvents ev;
	CompleteResult r;

	reset(); uint16_t c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21); // TC-019
	checkAbandonment(&S, TODAY, &ev);
	Character *c = findCharacter(&S, c1);
	CHECK(c->level == 0 && c->status == CHAR_ACTIVE && c->slotNumber == 1);

	reset(); c1 = mkChar("A", TODAY - 60, 2, 70, TODAY - 21);          // TC-020
	checkAbandonment(&S, TODAY, &ev);
	CHECK(findCharacter(&S, c1)->level == 1 && findCharacter(&S, c1)->status == CHAR_ACTIVE);

	reset(); c1 = mkChar("A", TODAY - 60, 0, 10, TODAY - 21);          // TC-021
	checkAbandonment(&S, TODAY, &ev);
	c = findCharacter(&S, c1);
	CHECK(c->status == CHAR_ABANDONED && c->slotNumber == 0);
	CHECK(firstFreeSlot(&S) == 1);

	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 20);          // TC-022
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 1), TODAY, &r);
	c = findCharacter(&S, c1);
	CHECK(c->lastMissionCompletedDate == TODAY && daysInactive(c, TODAY) == 0);
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 0);

	// Escalonado: 45 días nivel 2 -> nivel 0, activo, quedan 3 días usados
	reset(); c1 = mkChar("A", TODAY - 60, 2, 70, TODAY - 45);
	checkAbandonment(&S, TODAY, &ev);
	c = findCharacter(&S, c1);
	CHECK(ev.count == 1 && ev.items[0].characterId == c1);
	CHECK(ev.items[0].previousLevel == 2 && ev.items[0].newLevel == 0 && !ev.items[0].slotFreed);
	CHECK(c->level == 0 && c->status == CHAR_ACTIVE);
	CHECK(daysInactive(c, TODAY) == 3);

	// 43 días nivel 1 -> baja a 0 y el segundo periodo abandona
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 43);
	checkAbandonment(&S, TODAY, &ev);
	c = findCharacter(&S, c1);
	CHECK(c->status == CHAR_ABANDONED && c->slotNumber == 0 && c->heartsTotal == 25);

	// Idempotente: segundo check el mismo día no repite
	reset(); c1 = mkChar("A", TODAY - 60, 2, 70, TODAY - 21);
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 1 && findCharacter(&S, c1)->level == 1);
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 0 && findCharacter(&S, c1)->level == 1);
}

// ---------- Área 5: cancelación y vencidas ----------
static void area5(void) {
	CompleteResult r;
	int16_t pen;
	uint16_t nuevo = 0;

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);  // TC-023 reschedule
	uint16_t m = mkMission(c1, DIFF_EASY, TODAY + 2);
	CHECK(rescheduleMission(&S, m, TODAY + 10, TODAY, &pen, &nuevo) == ENGINE_OK);
	CHECK(findMission(&S, m)->status == MISSION_CANCELLED);
	CHECK(findCharacter(&S, c1)->heartsTotal == 7);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 1);
	Mission *nm = findMission(&S, nuevo);
	CHECK(nm && nm->status == MISSION_PENDING && nm->deadline == TODAY + 10 && nm->difficulty == DIFF_EASY);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);           // TC-024: vencida 2 días
	m = mkMission(c1, DIFF_EASY, TODAY - 2);
	CHECK(completeMission(&S, m, TODAY, &r) == ENGINE_OK);
	CHECK(r.heartsEarned == 4 && findMission(&S, m)->status == MISSION_COMPLETED);
	CHECK(findCharacter(&S, c1)->heartsTotal == 14);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 0);

	reset(); c1 = mkChar("A", TODAY - 30, 1, 20, TODAY - 1);           // TC-025
	m = mkMission(c1, DIFF_MEDIUM, TODAY - 1);
	acceptMissionLoss(&S, m, &pen);
	CHECK(pen == 5 && findCharacter(&S, c1)->heartsTotal == 15 && findCharacter(&S, c1)->level == 1);
}

// ---------- Área 6: boda ----------
static void area6(void) {
	CompleteResult r;
	AbandonmentEvents ev;

	reset();
	uint16_t c1 = mkChar("Gym", TODAY - 66, 2, 139, TODAY - 1);
	uint16_t pend = mkMission(c1, DIFF_MEDIUM, TODAY + 5); // pendiente que la boda cancela
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), TODAY, &r);

	Character *c = findCharacter(&S, c1);
	CHECK(c->heartsTotal == 144 && c->level == 3 && r.wedding);        // TC-026
	CHECK(S.happyEndingCount == 1);                                    // TC-027
	CHECK(strcmp(S.happyEndings[0].characterName, "Gym") == 0);
	CHECK(S.happyEndings[0].originalCharacterId == c1);
	CHECK(S.happyEndings[0].weddingDate == TODAY);
	CHECK(c->status == CHAR_HAPPY_ENDING && activeCharacterCount(&S) == 0);
	CHECK(firstFreeSlot(&S) == 1);                                     // TC-028
	uint16_t c2 = 0;
	CHECK(createCharacter(&S, "B", TODAY, &c2) == ENGINE_OK);
	CHECK(findCharacter(&S, c2)->slotNumber == 1);
	CHECK(findMission(&S, pend)->status == MISSION_CANCELLED);         // TC-029
	CHECK(findMission(&S, pend)->heartsAwarded == 0);                  // sin penalización
	CHECK(createMission(&S, c1, "x", DIFF_EASY, TODAY + 1, TODAY, 0) == ERR_CHARACTER_NOT_ACTIVE);
	checkAbandonment(&S, TODAY + 60, &ev);
	int evC1 = 0;
	for (int i = 0; i < ev.count; i++) if (ev.items[i].characterId == c1) evC1++;
	CHECK(evC1 == 0);
}

// ---------- Área 7: límites ----------
static void area7(void) {
	CompleteResult r;

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	mkMission(c1, DIFF_EASY, TODAY + 1);
	mkMission(c1, DIFF_EASY, TODAY + 2);
	uint16_t m3 = mkMission(c1, DIFF_EASY, TODAY + 3);
	CHECK(pendingMissionCount(&S, c1) == 3);                           // TC-031 (la 3ra entra)
	CHECK(createMission(&S, c1, "m4", DIFF_EASY, TODAY + 4, TODAY, 0) == ERR_PENDING_LIMIT); // TC-030
	CHECK(pendingMissionCount(&S, c1) == 3);
	completeMission(&S, m3, TODAY, &r);                                // TC-034
	CHECK(pendingMissionCount(&S, c1) == 2);
	CHECK(createMission(&S, c1, "m4", DIFF_EASY, TODAY + 4, TODAY, 0) == ENGINE_OK);

	reset();
	mkChar("A", TODAY - 3, 0, 0, TODAY - 1);
	mkChar("B", TODAY - 2, 0, 0, TODAY - 1);
	uint16_t c3 = mkChar("C", TODAY - 1, 0, 0, TODAY - 1);             // TC-033: slot 3
	CHECK(findCharacter(&S, c3)->slotNumber == 3);
	CHECK(createCharacter(&S, "D", TODAY, 0) == ERR_NO_FREE_SLOT);     // TC-032
	CHECK(activeCharacterCount(&S) == 3);
}

// ---------- Área 8: edge cases ----------
static void area8(void) {
	CompleteResult r;
	int16_t pen;
	AbandonmentEvents ev;

	CHECK(levelForHearts(19) == 0 && levelForHearts(20) == 1);         // TC-035

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 4, TODAY - 1);   // TC-036: clamp 0
	cancelMission(&S, mkMission(c1, DIFF_HARD, TODAY + 7), &pen);
	CHECK(findCharacter(&S, c1)->heartsTotal == 0);

	reset(); c1 = mkChar("Gym", TODAY - 66, 2, 139, TODAY - 1);        // TC-037
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 7), TODAY, &r);
	CHECK(createCharacter(&S, "B", TODAY, 0) == ENGINE_OK);
	CHECK(createCharacter(&S, "C", TODAY, 0) == ENGINE_OK);
	CHECK(createCharacter(&S, "D", TODAY, 0) == ENGINE_OK);
	CHECK(activeCharacterCount(&S) == 3 && S.happyEndingCount == 1);

	reset(); c1 = mkChar("A", TODAY - 60, 0, 10, TODAY - 21);          // TC-038
	checkAbandonment(&S, TODAY, &ev);
	uint16_t c2 = 0;
	CHECK(createCharacter(&S, "B", TODAY, &c2) == ENGINE_OK);
	CHECK(findCharacter(&S, c2)->slotNumber == 1 && activeCharacterCount(&S) == 1);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);            // TC-039: deadline hoy
	uint16_t m = 0;
	CHECK(createMission(&S, c1, "hoy", DIFF_EASY, TODAY, TODAY, &m) == ENGINE_OK);
	completeMission(&S, m, TODAY, &r);
	CHECK(r.heartsEarned == 5);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 0);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);           // TC-040: 1 día tarde
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY - 1), TODAY, &r);
	CHECK(r.heartsEarned == 4 && findCharacter(&S, c1)->heartsTotal == 14);

	reset(); c1 = mkChar("A", TODAY - 30, 2, 60, TODAY - 1);           // TC-041: 3 cancelaciones
	uint16_t ma = mkMission(c1, DIFF_EASY, TODAY + 1);
	uint16_t mb = mkMission(c1, DIFF_MEDIUM, TODAY + 2);
	uint16_t mc = mkMission(c1, DIFF_HARD, TODAY + 3);
	cancelMission(&S, ma, &pen); CHECK(findCharacter(&S, c1)->pendingCancellationScene == 1);
	cancelMission(&S, mb, &pen);
	cancelMission(&S, mc, &pen);
	Character *c = findCharacter(&S, c1);
	CHECK(c->heartsTotal == 44 && c->level == 2);
	CHECK(findMission(&S, ma)->status == MISSION_CANCELLED);
	CHECK(findMission(&S, mb)->status == MISSION_CANCELLED);
	CHECK(findMission(&S, mc)->status == MISSION_CANCELLED);

	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);          // TC-042
	checkAbandonment(&S, TODAY, &ev);
	CHECK(findCharacter(&S, c1)->level == 0 && findCharacter(&S, c1)->heartsTotal == 25);
}

// ---------- P4: derecho de réplica ----------
static void suiteP4(void) {
	CompleteResult r;
	int16_t pen;

	// Multiplicadores sobre medium (base 10) con hearts 10
	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY - 1), TODAY, &r);
	CHECK(r.heartsEarned == 8 && findCharacter(&S, c1)->heartsTotal == 18);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY - 3), TODAY, &r);
	CHECK(r.heartsEarned == 8);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY - 5), TODAY, &r);
	CHECK(r.heartsEarned == 5);
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	uint16_t m = mkMission(c1, DIFF_HARD, TODAY - 30);
	completeMission(&S, m, TODAY, &r);
	CHECK(r.heartsEarned == 5 && findMission(&S, m)->status == MISSION_COMPLETED);

	// Completar tarde reinicia el reloj
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY - 1), TODAY, &r);
	Character *c = findCharacter(&S, c1);
	CHECK(c->lastMissionCompletedDate == TODAY && c->inactivitySince == TODAY);

	// Completar tarde puede subir de nivel: 15 + 8 = 23
	reset(); c1 = mkChar("A", TODAY - 30, 0, 15, TODAY - 10);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, TODAY - 1), TODAY, &r);
	CHECK(r.leveledUp && r.newLevel == 1);

	// Aceptar la pérdida
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	m = mkMission(c1, DIFF_MEDIUM, TODAY - 1);
	CHECK(acceptMissionLoss(&S, m, &pen) == ENGINE_OK && pen == 5);
	CHECK(findMission(&S, m)->status == MISSION_FAILED);
	CHECK(findMission(&S, m)->heartsAwarded == -5);
	c = findCharacter(&S, c1);
	CHECK(c->heartsTotal == 5 && c->pendingCancellationScene == 1);
	CHECK(c->inactivitySince == TODAY - 10);                 // NO reinicia el reloj
	CHECK(acceptMissionLoss(&S, m, &pen) == ERR_MISSION_NOT_PENDING);

	// Vencidas pendientes cuentan para el tope de 3
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 10);
	uint16_t v1 = mkMission(c1, DIFF_EASY, TODAY - 3);
	mkMission(c1, DIFF_EASY, TODAY - 2);
	mkMission(c1, DIFF_EASY, TODAY - 1);
	CHECK(createMission(&S, c1, "m4", DIFF_EASY, TODAY + 1, TODAY, 0) == ERR_PENDING_LIMIT);
	acceptMissionLoss(&S, v1, &pen);
	CHECK(createMission(&S, c1, "m4", DIFF_EASY, TODAY + 1, TODAY, 0) == ENGINE_OK);
}

// ---------- P5: deadline mínimo hoy ----------
static void suiteP5(void) {
	CompleteResult r;
	int16_t pen;
	uint16_t nuevo = 0;

	reset(); uint16_t c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	uint16_t m = 0;
	CHECK(createMission(&S, c1, "hoy", DIFF_EASY, TODAY, TODAY, &m) == ENGINE_OK);
	CHECK(findMission(&S, m)->deadline == TODAY);
	completeMission(&S, m, TODAY, &r);
	CHECK(r.heartsEarned == 5);

	CHECK(createMission(&S, c1, "ayer", DIFF_EASY, TODAY - 1, TODAY, 0) == ERR_DEADLINE_IN_PAST);

	// reschedule a ayer: error SIN penalizar
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	m = mkMission(c1, DIFF_EASY, TODAY + 2);
	CHECK(rescheduleMission(&S, m, TODAY - 1, TODAY, &pen, &nuevo) == ERR_DEADLINE_IN_PAST);
	CHECK(findMission(&S, m)->status == MISSION_PENDING);
	CHECK(findCharacter(&S, c1)->heartsTotal == 10);

	// reschedule a hoy: válido
	CHECK(rescheduleMission(&S, m, TODAY, TODAY, &pen, &nuevo) == ENGINE_OK);
	CHECK(findMission(&S, nuevo)->deadline == TODAY);
}

// ---------- hearts: multiplicador, niveles, barra ----------
static void suiteHearts(void) {
	// lateMultiplier vía calcHeartsEarned (deadline fija, completado después)
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY) == 5);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY - 5) == 5);   // antes del deadline
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 1) == 4);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 3) == 4);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 4) == 3);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 7) == 3);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 8) == 2);
	CHECK(calcHeartsEarned(DIFF_EASY, TODAY, TODAY + 15) == 2);
	CHECK(calcHeartsEarned(DIFF_MEDIUM, TODAY, TODAY + 1) == 8);
	CHECK(calcHeartsEarned(DIFF_MEDIUM, TODAY, TODAY + 5) == 5);
	CHECK(calcHeartsEarned(DIFF_MEDIUM, TODAY, TODAY + 30) == 3);
	CHECK(calcHeartsEarned(DIFF_HARD, TODAY, TODAY + 1) == 14);
	CHECK(calcHeartsEarned(DIFF_HARD, TODAY, TODAY + 5) == 9);
	CHECK(calcHeartsEarned(DIFF_HARD, TODAY, TODAY + 30) == 5);

	CHECK(levelForHearts(0) == 0 && levelForHearts(19) == 0);
	CHECK(levelForHearts(20) == 1 && levelForHearts(59) == 1);
	CHECK(levelForHearts(60) == 2 && levelForHearts(139) == 2);
	CHECK(levelForHearts(140) == 3 && levelForHearts(500) == 3);

	int16_t cur, tot;
	CHECK(heartsToNextLevel(1, 25, &cur, &tot) && cur == 5 && tot == 40);
	CHECK(heartsToNextLevel(2, 75, &cur, &tot) && cur == 15 && tot == 80);
	CHECK(heartsToNextLevel(0, 0, &cur, &tot) && cur == 0 && tot == 20);
	CHECK(!heartsToNextLevel(3, 150, &cur, &tot));
	CHECK(heartsToNextLevel(1, 0, &cur, &tot) && cur == 0 && tot == 40);   // clamp M2
	CHECK(heartsToNextLevel(1, 75, &cur, &tot) && cur == 40 && tot == 40); // clamp M2
}

// ---------- consultas del motor web ----------
static void suiteQueries(void) {
	CompleteResult r;
	int16_t pen;
	AbandonmentEvents ev;

	// acknowledge de escenas
	reset(); uint16_t c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);
	checkAbandonment(&S, TODAY, &ev);
	CHECK(findCharacter(&S, c1)->pendingAbandonmentScene == 1);
	acknowledgeAbandonmentScene(&S, c1);
	CHECK(findCharacter(&S, c1)->pendingAbandonmentScene == 0);
	checkAbandonment(&S, TODAY, &ev);
	CHECK(ev.count == 0);

	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_EASY, TODAY + 1), &pen);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 1);
	acknowledgeCancellationScene(&S, c1);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 0);

	// completedCount cuenta solo completadas del personaje
	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	uint16_t c2 = mkChar("B", TODAY - 30, 0, 0, TODAY - 1);
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 1), TODAY, &r);
	completeMission(&S, mkMission(c1, DIFF_EASY, TODAY + 2), TODAY, &r);
	cancelMission(&S, mkMission(c1, DIFF_EASY, TODAY + 3), &pen);
	mkMission(c1, DIFF_EASY, TODAY + 4);
	completeMission(&S, mkMission(c2, DIFF_EASY, TODAY + 1), TODAY, &r);
	CHECK(findCharacter(&S, c1)->completedCount == 2);
	CHECK(findCharacter(&S, c2)->completedCount == 1);

	// daysTogether / isAtRisk / riskLevel / daysUntilLeaving
	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 13);
	CHECK(daysTogether(findCharacter(&S, c1), TODAY) == 30);
	CHECK(!isAtRisk(findCharacter(&S, c1), TODAY));                      // 13
	findCharacter(&S, c1)->inactivitySince = TODAY - 14;
	CHECK(isAtRisk(findCharacter(&S, c1), TODAY));
	findCharacter(&S, c1)->inactivitySince = TODAY - 20;
	CHECK(isAtRisk(findCharacter(&S, c1), TODAY));
	findCharacter(&S, c1)->inactivitySince = TODAY - 21;
	CHECK(!isAtRisk(findCharacter(&S, c1), TODAY));

	CHECK(riskLevel(13) == RISK_NONE && riskLevel(14) == RISK_SOFT);
	CHECK(riskLevel(17) == RISK_SOFT && riskLevel(18) == RISK_STRONG);
	CHECK(riskLevel(20) == RISK_STRONG && riskLevel(21) == RISK_NONE);
	CHECK(daysUntilLeaving(14) == 7 && daysUntilLeaving(18) == 3);
	CHECK(daysUntilLeaving(20) == 1 && daysUntilLeaving(21) == 0 && daysUntilLeaving(30) == 0);

	// deleteCharacter
	reset(); c1 = mkChar("A", TODAY - 30, 0, 0, TODAY - 1);
	c2 = mkChar("B", TODAY - 30, 0, 0, TODAY - 1);
	mkMission(c1, DIFF_EASY, TODAY + 1);
	mkMission(c1, DIFF_EASY, TODAY + 2);
	uint16_t m3 = mkMission(c2, DIFF_EASY, TODAY + 3);
	CHECK(firstFreeSlot(&S) == 3);
	CHECK(deleteCharacter(&S, c1) == ENGINE_OK);
	CHECK(findCharacter(&S, c1) == 0);
	CHECK(pendingMissionCount(&S, c1) == 0);
	CHECK(findMission(&S, m3) != 0);                                     // las ajenas quedan
	CHECK(firstFreeSlot(&S) == 1);
	CHECK(deleteCharacter(&S, 999) == ERR_CHARACTER_NOT_FOUND);

	// apariencia (P12): cosmética, no toca mecánica
	reset(); c1 = mkChar("A", TODAY - 30, 1, 25, TODAY - 1);
	CHECK(setAppearance(&S, c1, 2, 1, 3, 0) == ENGINE_OK);
	Character *c = findCharacter(&S, c1);
	CHECK(c->appearanceBase == 2 && c->appearancePalette == 1 &&
	      c->appearanceAccessory == 3 && c->appearanceExpression == 0);
	CHECK(c->level == 1 && c->heartsTotal == 25);
	CHECK(setAppearance(&S, 999, 0, 0, 0, 0) == ERR_CHARACTER_NOT_FOUND);
}

// ---------- startup ----------
static void suiteStartup(void) {
	StartupScenes sc;
	WeekCloseResult wk;
	int16_t pen;

	// Estado limpio
	reset(); mkChar("A", TODAY - 5, 0, 10, TODAY - 1);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 0);

	// Vencida NO se auto-falla (P4)
	reset(); uint16_t c1 = mkChar("A", TODAY - 5, 0, 10, TODAY - 1);
	uint16_t m = mkMission(c1, DIFF_MEDIUM, TODAY - 2);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 0);
	CHECK(findMission(&S, m)->status == MISSION_PENDING);
	CHECK(findCharacter(&S, c1)->heartsTotal == 10);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 0);

	// 21 días inactivo genera la escena
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1 && sc.items[0].kind == SCENE_ABANDONMENT && sc.items[0].characterId == c1);

	// Vencida pendiente no detiene el reloj
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);
	m = mkMission(c1, DIFF_EASY, TODAY - 10);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1 && sc.items[0].kind == SCENE_ABANDONMENT);
	CHECK(findCharacter(&S, c1)->level == 0);
	CHECK(findMission(&S, m)->status == MISSION_PENDING);

	// Re-hidratación: flag sin evento nuevo
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 1);
	findCharacter(&S, c1)->pendingAbandonmentScene = 1;
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1 && sc.items[0].kind == SCENE_ABANDONMENT);

	// Flag de personaje ya abandonado también se re-hidrata
	reset(); c1 = mkChar("A", TODAY - 60, 0, 10, TODAY - 1);
	Character *c = findCharacter(&S, c1);
	c->status = CHAR_ABANDONED; c->slotNumber = 0; c->pendingAbandonmentScene = 1;
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1 && sc.items[0].characterId == c1);

	// Cancelación usa la cerrada MÁS RECIENTE
	reset(); c1 = mkChar("A", TODAY - 30, 0, 20, TODAY - 1);
	uint16_t m1 = mkMission(c1, DIFF_EASY, TODAY - 3);
	uint16_t m2 = mkMission(c1, DIFF_EASY, TODAY + 1);
	uint16_t m3 = mkMission(c1, DIFF_EASY, TODAY + 2);
	acceptMissionLoss(&S, m1, &pen);         // failed (más vieja)
	cancelMission(&S, m2, &pen);             // cancelled (más reciente)
	CompleteResult r;
	completeMission(&S, m3, TODAY, &r);      // completed (no cuenta)
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1 && sc.items[0].kind == SCENE_CANCELLATION && sc.items[0].missionId == m2);

	// Flag de cancelación sin misión cerrada: se limpia sin escena
	reset(); c1 = mkChar("A", TODAY - 30, 0, 10, TODAY - 1);
	findCharacter(&S, c1)->pendingCancellationScene = 1;
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 0);
	CHECK(findCharacter(&S, c1)->pendingCancellationScene == 0);

	// No duplica abandono cuando el check de hoy ya la generó
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 1);

	// Varios personajes: abandono de uno + cancelación de otro
	reset(); c1 = mkChar("A", TODAY - 60, 1, 25, TODAY - 21);
	uint16_t c2 = mkChar("B", TODAY - 30, 0, 10, TODAY - 1);
	m = mkMission(c2, DIFF_EASY, TODAY + 1);
	cancelMission(&S, m, &pen);
	buildStartup(&S, TODAY, &sc, &wk);
	CHECK(sc.count == 2);
	CHECK(sc.items[0].kind == SCENE_ABANDONMENT && sc.items[0].characterId == c1);
	CHECK(sc.items[1].kind == SCENE_CANCELLATION && sc.items[1].characterId == c2 && sc.items[1].missionId == m);
}

// ---------- P12: ranking semanal ----------
static void suiteRanking(void) {
	CompleteResult r;
	int16_t pen;
	WeekCloseResult wk;

	// weekStart: lunes de la semana
	CHECK(weekStart(MONDAY) == MONDAY);
	CHECK(weekStart(MONDAY + 6) == MONDAY);   // domingo
	CHECK(weekStart(MONDAY + 7) == MONDAY + 7);
	CHECK(weekStart(MONDAY - 1) == MONDAY - 7);

	// Acumulador de netos: completar suma, penalizar resta (nominal)
	gameInit(&S, MONDAY);
	uint16_t c1 = mkChar("A", MONDAY - 30, 0, 0, MONDAY - 1);
	completeMission(&S, mkMission(c1, DIFF_EASY, MONDAY + 1), MONDAY, &r);
	CHECK(findCharacter(&S, c1)->weeklyHearts == 5);
	cancelMission(&S, mkMission(c1, DIFF_MEDIUM, MONDAY + 2), &pen);
	CHECK(findCharacter(&S, c1)->weeklyHearts == 0);
	// heartsTotal clampea en 0 pero el neto semanal sí baja
	findCharacter(&S, c1)->heartsTotal = 2;
	cancelMission(&S, mkMission(c1, DIFF_HARD, MONDAY + 3), &pen);
	CHECK(findCharacter(&S, c1)->heartsTotal == 0);
	CHECK(findCharacter(&S, c1)->weeklyHearts == -8);

	// Cierre de semana: gana el de más netos, se congela y resetea
	gameInit(&S, MONDAY);
	c1 = mkChar("A", MONDAY - 30, 0, 0, MONDAY - 1);
	uint16_t c2 = mkChar("B", MONDAY - 30, 0, 0, MONDAY - 1);
	completeMission(&S, mkMission(c1, DIFF_MEDIUM, MONDAY + 1), MONDAY, &r); // A: 10
	completeMission(&S, mkMission(c2, DIFF_EASY, MONDAY + 1), MONDAY, &r);   // B: 5
	CHECK(rankingLeader(&S) == c1);
	rankingWeeklyCheck(&S, MONDAY + 7, &wk);
	CHECK(wk.closed && wk.winnerCharacterId == c1);
	CHECK(S.lastWeekWinnerId == c1);
	CHECK(findCharacter(&S, c1)->weeksWon == 1);
	CHECK(findCharacter(&S, c1)->weeklyHearts == 0 && findCharacter(&S, c2)->weeklyHearts == 0);
	CHECK(S.weekAnchor == MONDAY + 7);

	// Misma semana: no cierra
	rankingWeeklyCheck(&S, MONDAY + 9, &wk);
	CHECK(!wk.closed);

	// Nadie con neto > 0: sin ganador
	gameInit(&S, MONDAY);
	c1 = mkChar("A", MONDAY - 30, 0, 10, MONDAY - 1);
	cancelMission(&S, mkMission(c1, DIFF_EASY, MONDAY + 1), &pen); // neto -3
	rankingWeeklyCheck(&S, MONDAY + 7, &wk);
	CHECK(wk.closed && wk.winnerCharacterId == 0 && S.lastWeekWinnerId == 0);
	CHECK(findCharacter(&S, c1)->weeksWon == 0);

	// Empate: gana el más recientemente activo; si persiste, el creado antes
	gameInit(&S, MONDAY);
	c1 = mkChar("A", MONDAY - 30, 0, 0, MONDAY - 5);
	c2 = mkChar("B", MONDAY - 20, 0, 0, MONDAY - 1);   // más recientemente activo
	findCharacter(&S, c1)->weeklyHearts = 10;
	findCharacter(&S, c2)->weeklyHearts = 10;
	CHECK(rankingLeader(&S) == c2);
	findCharacter(&S, c2)->inactivitySince = MONDAY - 5; // empata actividad
	CHECK(rankingLeader(&S) == c1);                      // creado antes
	// Abandonado no compite aunque tenga más netos
	findCharacter(&S, c2)->weeklyHearts = 50;
	findCharacter(&S, c2)->status = CHAR_ABANDONED;
	CHECK(rankingLeader(&S) == c1);

	// Gap multi-semana: un solo cierre al reabrir
	gameInit(&S, MONDAY);
	c1 = mkChar("A", MONDAY - 30, 0, 0, MONDAY - 1);
	completeMission(&S, mkMission(c1, DIFF_EASY, MONDAY + 1), MONDAY, &r);
	rankingWeeklyCheck(&S, MONDAY + 21, &wk);
	CHECK(wk.closed && wk.winnerCharacterId == c1);
	CHECK(S.weekAnchor == MONDAY + 21);
	rankingWeeklyCheck(&S, MONDAY + 22, &wk);
	CHECK(!wk.closed);

	// buildStartup integra el cierre de semana
	gameInit(&S, MONDAY);
	c1 = mkChar("A", MONDAY - 30, 0, 0, MONDAY - 1);
	completeMission(&S, mkMission(c1, DIFF_EASY, MONDAY + 1), MONDAY, &r);
	StartupScenes sc;
	buildStartup(&S, MONDAY + 7, &sc, &wk);
	CHECK(wk.closed && wk.winnerCharacterId == c1);
}

int main(void) {
	area1();
	area2();
	area3();
	area4();
	area5();
	area6();
	area7();
	area8();
	suiteP4();
	suiteP5();
	suiteHearts();
	suiteQueries();
	suiteStartup();
	suiteRanking();

	if (failures == 0) {
		printf("ENGINE TESTS OK (%d checks)\n", checks);
		return 0;
	}
	printf("ENGINE TESTS: %d fallos de %d checks\n", failures, checks);
	return 1;
}
