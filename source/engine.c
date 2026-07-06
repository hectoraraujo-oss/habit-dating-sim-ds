#include "engine.h"

#include <string.h>

static const int16_t HEARTS_BY_DIFFICULTY[3] = {5, 10, 18};
static const int16_t CANCEL_PENALTY[3] = {3, 5, 8};
static const int16_t LEVEL_THRESHOLDS[4] = {0, 20, 60, 140};

// ---------- helpers ----------

static void copy_name(char *dst, const char *src) {
	strncpy(dst, src ? src : "", NAME_LEN - 1);
	dst[NAME_LEN - 1] = '\0';
}

static uint16_t next_id(GameState *s) {
	if (s->nextId == 0) s->nextId = 1;
	return s->nextId++;
}

// ---------- init ----------

void gameInit(GameState *s, GameDate today) {
	memset(s, 0, sizeof(*s));
	s->nextId = 1;
	s->weekAnchor = weekStart(today);
}

// ---------- fechas ----------

GameDate weekStart(GameDate d) {
	// 1970-01-01 (GameDate 0) fue jueves. dow: 0=lunes .. 6=domingo.
	int32_t dow = (int32_t)(((d % 7) + 7 + 3) % 7);
	return d - dow;
}

// ---------- consultas ----------

Character *findCharacter(GameState *s, uint16_t id) {
	if (id == 0) return 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
		if (s->characters[i].id == id) return &s->characters[i];
	return 0;
}

Mission *findMission(GameState *s, uint16_t id) {
	if (id == 0) return 0;
	for (int i = 0; i < MAX_MISSIONS; i++)
		if (s->missions[i].id == id) return &s->missions[i];
	return 0;
}

int activeCharacterCount(const GameState *s) {
	int n = 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
		if (s->characters[i].id && s->characters[i].status == CHAR_ACTIVE) n++;
	return n;
}

int pendingMissionCount(const GameState *s, uint16_t characterId) {
	int n = 0;
	for (int i = 0; i < MAX_MISSIONS; i++)
		if (s->missions[i].id && s->missions[i].characterId == characterId &&
		    s->missions[i].status == MISSION_PENDING) n++;
	return n;
}

uint8_t firstFreeSlot(const GameState *s) {
	bool used[4] = {false, false, false, false};
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		const Character *c = &s->characters[i];
		if (c->id && c->status == CHAR_ACTIVE && c->slotNumber >= 1 && c->slotNumber <= 3)
			used[c->slotNumber] = true;
	}
	for (uint8_t slot = 1; slot <= 3; slot++)
		if (!used[slot]) return slot;
	return 0;
}

int32_t daysInactive(const Character *c, GameDate today) {
	int32_t d = today - c->inactivitySince;
	return d < 0 ? 0 : d;
}

int32_t daysTogether(const Character *c, GameDate today) {
	int32_t d = today - c->createdDate;
	return d < 0 ? 0 : d;
}

bool isAtRisk(const Character *c, GameDate today) {
	if (c->status != CHAR_ACTIVE) return false;
	int32_t d = daysInactive(c, today);
	return d >= AT_RISK_DAYS && d < ABANDONMENT_DAYS;
}

RiskLevel riskLevel(int32_t d) {
	if (d < AT_RISK_DAYS || d >= ABANDONMENT_DAYS) return RISK_NONE;
	return d >= RISK_STRONG_DAYS ? RISK_STRONG : RISK_SOFT;
}

int32_t daysUntilLeaving(int32_t d) {
	int32_t left = ABANDONMENT_DAYS - d;
	return left < 0 ? 0 : left;
}

// ---------- corazones / niveles ----------

int16_t heartsForDifficulty(Difficulty d) { return HEARTS_BY_DIFFICULTY[d]; }
int16_t cancelPenaltyFor(Difficulty d) { return CANCEL_PENALTY[d]; }

int16_t calcHeartsEarned(Difficulty d, GameDate deadline, GameDate completedAt) {
	int32_t late = completedAt - deadline;
	if (late < 0) late = 0;
	// Multiplicador P4 sin floats: {num,den} con CEILING entero. Piso 25%.
	int num, den;
	if (late == 0)      { num = 1; den = 1; }
	else if (late <= 3) { num = 3; den = 4; }
	else if (late <= 7) { num = 1; den = 2; }
	else                { num = 1; den = 4; }
	int base = HEARTS_BY_DIFFICULTY[d];
	return (int16_t)((base * num + den - 1) / den);
}

uint8_t levelForHearts(int16_t hearts) {
	uint8_t level = 0;
	for (int n = 1; n <= MAX_LEVEL; n++)
		if (hearts >= LEVEL_THRESHOLDS[n]) level = (uint8_t)n;
	return level;
}

bool heartsToNextLevel(uint8_t level, int16_t hearts, int16_t *current, int16_t *total) {
	if (level >= MAX_LEVEL) return false;
	int16_t floor_ = LEVEL_THRESHOLDS[level];
	int16_t tot = (int16_t)(LEVEL_THRESHOLDS[level + 1] - floor_);
	int16_t cur = (int16_t)(hearts - floor_);
	if (cur < 0) cur = 0;           // clamp de presentación (QA M2)
	if (cur > tot) cur = tot;
	if (current) *current = cur;
	if (total) *total = tot;
	return true;
}

// Delta de corazones + acumulador semanal (P12: deltas nominales, el neto
// semanal puede ser negativo aunque heartsTotal clampee en 0).
static void apply_hearts_delta(Character *c, int16_t delta) {
	int32_t h = (int32_t)c->heartsTotal + delta;
	c->heartsTotal = (int16_t)(h < 0 ? 0 : h);
	c->weeklyHearts = (int16_t)(c->weeklyHearts + delta);
}

static void apply_penalty(GameState *s, uint16_t characterId, int16_t penalty) {
	Character *c = findCharacter(s, characterId);
	if (!c) return;                 // misión huérfana: estado intacto (como el TS)
	apply_hearts_delta(c, (int16_t)-penalty);
	c->pendingCancellationScene = 1;
	// NO toca level ni inactivitySince.
}

// ---------- asignación de registros ----------

static Character *alloc_character(GameState *s) {
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
		if (s->characters[i].id == 0) return &s->characters[i];
	// Recicla el histórico más viejo (no activo, sin escenas pendientes).
	Character *victim = 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &s->characters[i];
		if (c->status == CHAR_ACTIVE || c->pendingAbandonmentScene || c->pendingCancellationScene)
			continue;
		if (!victim || c->id < victim->id) victim = c;
	}
	return victim;
}

static Mission *alloc_mission(GameState *s) {
	for (int i = 0; i < MAX_MISSIONS; i++)
		if (s->missions[i].id == 0) return &s->missions[i];
	// Recicla la misión cerrada más vieja. Con tope de 9 pendientes siempre
	// hay cerradas cuando el array de 16 se llena.
	Mission *victim = 0;
	for (int i = 0; i < MAX_MISSIONS; i++) {
		Mission *m = &s->missions[i];
		if (m->status == MISSION_PENDING) continue;
		if (!victim || m->id < victim->id) victim = m;
	}
	return victim;
}

// ---------- mutaciones ----------

EngineError createCharacter(GameState *s, const char *name, GameDate today, uint16_t *outId) {
	uint8_t slot = firstFreeSlot(s);
	if (slot == 0) return ERR_NO_FREE_SLOT;
	Character *c = alloc_character(s);
	if (!c) return ERR_NO_SPACE;
	memset(c, 0, sizeof(*c));
	c->id = next_id(s);
	copy_name(c->name, name);
	c->slotNumber = slot;
	c->status = CHAR_ACTIVE;
	c->createdDate = today;
	c->inactivitySince = today;
	if (outId) *outId = c->id;
	return ENGINE_OK;
}

EngineError deleteCharacter(GameState *s, uint16_t characterId) {
	Character *c = findCharacter(s, characterId);
	if (!c) return ERR_CHARACTER_NOT_FOUND;
	for (int i = 0; i < MAX_MISSIONS; i++)
		if (s->missions[i].characterId == characterId)
			memset(&s->missions[i], 0, sizeof(Mission));
	memset(c, 0, sizeof(*c));       // happyEndings se conservan
	return ENGINE_OK;
}

EngineError setAppearance(GameState *s, uint16_t characterId,
                          uint8_t base, uint8_t palette, uint8_t accessory, uint8_t expression) {
	Character *c = findCharacter(s, characterId);
	if (!c) return ERR_CHARACTER_NOT_FOUND;
	c->appearanceBase = base;
	c->appearancePalette = palette;
	c->appearanceAccessory = accessory;
	c->appearanceExpression = expression;
	return ENGINE_OK;
}

EngineError createMission(GameState *s, uint16_t characterId, const char *name,
                          Difficulty d, GameDate deadline, GameDate today, uint16_t *outId) {
	Character *c = findCharacter(s, characterId);
	if (!c) return ERR_CHARACTER_NOT_FOUND;
	if (c->status != CHAR_ACTIVE) return ERR_CHARACTER_NOT_ACTIVE;
	if (pendingMissionCount(s, characterId) >= MAX_PENDING_PER_CHARACTER)
		return ERR_PENDING_LIMIT;   // las vencidas pendientes CUENTAN (P4)
	if (deadline - today < 0) return ERR_DEADLINE_IN_PAST; // deadline hoy es válido (P5)
	Mission *m = alloc_mission(s);
	if (!m) return ERR_NO_SPACE;
	memset(m, 0, sizeof(*m));
	m->id = next_id(s);
	m->characterId = characterId;
	copy_name(m->name, name);
	m->difficulty = (uint8_t)d;
	m->status = MISSION_PENDING;
	m->deadline = deadline;
	if (outId) *outId = m->id;
	return ENGINE_OK;
}

static void apply_wedding(GameState *s, Character *c, GameDate today) {
	c->status = CHAR_HAPPY_ENDING;
	c->slotNumber = 0;
	// Pendientes se cancelan SIN penalización (TC-029), heartsAwarded queda null.
	for (int i = 0; i < MAX_MISSIONS; i++) {
		Mission *m = &s->missions[i];
		if (m->id && m->characterId == c->id && m->status == MISSION_PENDING)
			m->status = MISSION_CANCELLED;
	}
	if (s->happyEndingCount >= MAX_HAPPY_ENDINGS) {
		// ponytail: lleno = se descarta el más viejo; 16 bodas dan para años.
		memmove(&s->happyEndings[0], &s->happyEndings[1],
		        sizeof(HappyEnding) * (MAX_HAPPY_ENDINGS - 1));
		s->happyEndingCount = MAX_HAPPY_ENDINGS - 1;
	}
	HappyEnding *h = &s->happyEndings[s->happyEndingCount++];
	memset(h, 0, sizeof(*h));
	h->id = next_id(s);
	copy_name(h->characterName, c->name);
	h->originalCharacterId = c->id;
	h->weddingDate = today;
	h->completedCount = c->completedCount;
}

EngineError completeMission(GameState *s, uint16_t missionId, GameDate today, CompleteResult *out) {
	Mission *m = findMission(s, missionId);
	if (!m) return ERR_MISSION_NOT_FOUND;
	if (m->status != MISSION_PENDING) return ERR_MISSION_NOT_PENDING;
	Character *c = findCharacter(s, m->characterId);
	if (!c) return ERR_MISSION_NOT_FOUND; // misión huérfana

	int16_t earned = calcHeartsEarned((Difficulty)m->difficulty, m->deadline, today);
	m->status = MISSION_COMPLETED;
	m->completedDate = today;
	m->heartsAwarded = earned;

	apply_hearts_delta(c, earned);
	c->lastMissionCompletedDate = today;
	c->inactivitySince = today;     // completar SÍ reinicia el reloj de abandono
	c->completedCount++;

	bool leveledUp = false;
	// Sube DE A UN NIVEL por misión aunque cruce dos umbrales (TC-012).
	if (c->level < MAX_LEVEL && c->heartsTotal >= LEVEL_THRESHOLDS[c->level + 1]) {
		c->level++;
		leveledUp = true;
	}
	bool wedding = leveledUp && c->level == MAX_LEVEL;
	if (wedding) apply_wedding(s, c, today);

	if (out) {
		out->heartsEarned = earned;
		out->leveledUp = leveledUp;
		out->newLevel = c->level;
		out->wedding = wedding;
	}
	return ENGINE_OK;
}

static EngineError close_mission(GameState *s, uint16_t missionId, MissionStatus final,
                                 int16_t *outPenalty) {
	Mission *m = findMission(s, missionId);
	if (!m) return ERR_MISSION_NOT_FOUND;
	if (m->status != MISSION_PENDING) return ERR_MISSION_NOT_PENDING;
	int16_t penalty = CANCEL_PENALTY[m->difficulty];
	m->status = (uint8_t)final;
	m->heartsAwarded = (int16_t)-penalty;
	apply_penalty(s, m->characterId, penalty);
	if (outPenalty) *outPenalty = penalty;
	return ENGINE_OK;
}

EngineError cancelMission(GameState *s, uint16_t missionId, int16_t *outPenalty) {
	// No recibe fecha: no toca el reloj de abandono.
	return close_mission(s, missionId, MISSION_CANCELLED, outPenalty);
}

EngineError acceptMissionLoss(GameState *s, uint16_t missionId, int16_t *outPenalty) {
	// P4 "aceptar la pérdida": failed + penalización. NO reinicia inactivitySince.
	return close_mission(s, missionId, MISSION_FAILED, outPenalty);
}

EngineError rescheduleMission(GameState *s, uint16_t missionId, GameDate newDeadline,
                              GameDate today, int16_t *outPenalty, uint16_t *outNewMissionId) {
	Mission *m = findMission(s, missionId);
	if (!m) return ERR_MISSION_NOT_FOUND;
	// Se valida ANTES de cancelar: no penaliza si va a fallar (TC de P5).
	if (newDeadline - today < 0) return ERR_DEADLINE_IN_PAST;
	uint16_t characterId = m->characterId;
	Difficulty diff = (Difficulty)m->difficulty;
	char name[NAME_LEN];
	memcpy(name, m->name, NAME_LEN);

	EngineError e = cancelMission(s, missionId, outPenalty);
	if (e != ENGINE_OK) return e;
	return createMission(s, characterId, name, diff, newDeadline, today, outNewMissionId);
}

void checkAbandonment(GameState *s, GameDate today, AbandonmentEvents *out) {
	if (out) out->count = 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &s->characters[i];
		if (c->id == 0 || c->status != CHAR_ACTIVE) continue;
		int32_t periods = daysInactive(c, today) / ABANDONMENT_DAYS;
		if (periods <= 0) continue;

		uint8_t prevLevel = c->level;
		uint8_t level = c->level;
		bool abandoned = false;
		int32_t applied = 0;
		for (int32_t p = 0; p < periods; p++) {
			applied++;
			if (level > 0) level--;
			else { abandoned = true; break; } // el periodo en nivel 0 consume el abandono
		}
		if (abandoned) {
			c->level = level;
			c->status = CHAR_ABANDONED;
			c->slotNumber = 0;
			c->pendingAbandonmentScene = 1;
			// Pendientes -> failed SIN penalización extra (heartsAwarded queda null).
			for (int j = 0; j < MAX_MISSIONS; j++) {
				Mission *m = &s->missions[j];
				if (m->id && m->characterId == c->id && m->status == MISSION_PENDING)
					m->status = MISSION_FAILED;
			}
		} else {
			c->level = level;
			c->pendingAbandonmentScene = 1;
			// El ancla avanza: el check es idempotente y acumulable.
			c->inactivitySince += applied * ABANDONMENT_DAYS;
		}
		// heartsTotal NUNCA cambia por abandono (TC-017/TC-042).
		if (out && out->count < MAX_CHARACTER_RECORDS) {
			AbandonmentEvent *ev = &out->items[out->count++];
			ev->characterId = c->id;
			ev->previousLevel = prevLevel;
			ev->newLevel = level;
			ev->slotFreed = abandoned;
		}
	}
}

void acknowledgeAbandonmentScene(GameState *s, uint16_t characterId) {
	Character *c = findCharacter(s, characterId);
	if (c) c->pendingAbandonmentScene = 0;
}

void acknowledgeCancellationScene(GameState *s, uint16_t characterId) {
	Character *c = findCharacter(s, characterId);
	if (c) c->pendingCancellationScene = 0;
}

// ---------- ranking semanal (P12) ----------

// Desempate determinista: más weeklyHearts; luego más recientemente activo
// (inactivitySince mayor); luego creado antes; luego slot menor.
static bool beats(const Character *a, const Character *b) {
	if (a->weeklyHearts != b->weeklyHearts) return a->weeklyHearts > b->weeklyHearts;
	if (a->inactivitySince != b->inactivitySince) return a->inactivitySince > b->inactivitySince;
	if (a->createdDate != b->createdDate) return a->createdDate < b->createdDate;
	return a->slotNumber < b->slotNumber;
}

uint16_t rankingLeader(GameState *s) {
	Character *best = 0;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &s->characters[i];
		if (c->id == 0 || c->status != CHAR_ACTIVE) continue;
		if (c->weeklyHearts <= 0) continue; // el #1 solo es #1 con neto > 0
		if (!best || beats(c, best)) best = c;
	}
	return best ? best->id : 0;
}

void rankingWeeklyCheck(GameState *s, GameDate today, WeekCloseResult *out) {
	if (out) { out->closed = false; out->winnerCharacterId = 0; }
	GameDate current = weekStart(today);
	if (s->weekAnchor == 0) s->weekAnchor = current;
	if (current <= s->weekAnchor) return;

	uint16_t winner = rankingLeader(s); // el podio de cierre solo lista activos
	if (winner) {
		Character *w = findCharacter(s, winner);
		if (w) w->weeksWon++;
	}
	s->lastWeekWinnerId = winner;
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++)
		s->characters[i].weeklyHearts = 0;
	s->weekAnchor = current;
	if (out) { out->closed = true; out->winnerCharacterId = winner; }
}

// ---------- arranque ----------

// Misión cerrada (failed/cancelled) más reciente del personaje = id mayor.
static Mission *latest_closed_mission(GameState *s, uint16_t characterId) {
	Mission *best = 0;
	for (int i = 0; i < MAX_MISSIONS; i++) {
		Mission *m = &s->missions[i];
		if (m->id == 0 || m->characterId != characterId) continue;
		if (m->status != MISSION_FAILED && m->status != MISSION_CANCELLED) continue;
		if (!best || m->id > best->id) best = m;
	}
	return best;
}

void buildStartup(GameState *s, GameDate today, StartupScenes *scenes, WeekCloseResult *week) {
	// ponytail: cierre de semana ANTES del abandono; el podio se congela con
	// los estados de la última sesión jugada. Supuesto anotado en PROGRESO.
	WeekCloseResult wk;
	rankingWeeklyCheck(s, today, &wk);
	if (week) *week = wk;

	AbandonmentEvents ev;
	checkAbandonment(s, today, &ev);

	if (scenes) scenes->count = 0;
	if (!scenes) return;

	bool queued[MAX_CHARACTER_RECORDS] = {false};
	for (int i = 0; i < ev.count; i++) {
		if (scenes->count < (uint8_t)(MAX_CHARACTER_RECORDS * 2)) {
			StartupScene *sc = &scenes->items[scenes->count++];
			sc->kind = SCENE_ABANDONMENT;
			sc->characterId = ev.items[i].characterId;
			sc->missionId = 0;
		}
	}
	for (int i = 0; i < MAX_CHARACTER_RECORDS; i++) {
		Character *c = &s->characters[i];
		if (c->id == 0) continue;
		if (c->pendingAbandonmentScene) {
			bool already = false;
			for (int j = 0; j < ev.count; j++)
				if (ev.items[j].characterId == c->id) already = true;
			if (!already && scenes->count < (uint8_t)(MAX_CHARACTER_RECORDS * 2)) {
				StartupScene *sc = &scenes->items[scenes->count++];
				sc->kind = SCENE_ABANDONMENT;
				sc->characterId = c->id;
				sc->missionId = 0;
			}
			queued[i] = true;
		}
		if (c->pendingCancellationScene) {
			Mission *m = latest_closed_mission(s, c->id);
			if (m) {
				if (scenes->count < (uint8_t)(MAX_CHARACTER_RECORDS * 2)) {
					StartupScene *sc = &scenes->items[scenes->count++];
					sc->kind = SCENE_CANCELLATION;
					sc->characterId = c->id;
					sc->missionId = m->id;
				}
			} else {
				c->pendingCancellationScene = 0; // flag huérfano: se limpia sin escena
			}
		}
	}
	(void)queued;
}
