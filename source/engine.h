// Motor del juego Habit Dating Sim. LÓGICA PURA:
// - No llama a libnds, ni al RTC, ni al save. La fecha entra como GameDate.
// - Sin malloc: arrays fijos, IDs enteros autoincrementales (0 = vacío/null).
// - Portado del motor web vivo (habit-dating-sim-app/src/game/*) con P4
//   (derecho de réplica), P5 (deadline hoy válido) y P12 (ranking semanal +
//   apariencia cosmética). Los tests en test/engine_test.c son la spec.
#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Fecha = días enteros desde epoch (1970-01-01). 0 = null/sin fecha.
// La resta de dos GameDate ES daysBetween.
typedef int32_t GameDate;

#define NAME_LEN 32
#define MAX_CHARACTERS 3            // activos a la vez (slots 1..3)
#define MAX_CHARACTER_RECORDS 8     // activos + históricos (abandonados/boda)
#define MAX_MISSIONS 16             // pendientes (máx 9) + historial reciente
#define MAX_HAPPY_ENDINGS 16
#define MAX_PENDING_PER_CHARACTER 3
#define MAX_LEVEL 3
#define ABANDONMENT_DAYS 21
#define AT_RISK_DAYS 14
#define RISK_STRONG_DAYS 18

typedef enum { DIFF_EASY = 0, DIFF_MEDIUM = 1, DIFF_HARD = 2 } Difficulty;
typedef enum { CHAR_ACTIVE = 0, CHAR_HAPPY_ENDING = 1, CHAR_ABANDONED = 2 } CharacterStatus;
typedef enum { MISSION_PENDING = 0, MISSION_COMPLETED = 1, MISSION_FAILED = 2, MISSION_CANCELLED = 3 } MissionStatus;
typedef enum { RISK_NONE = 0, RISK_SOFT = 1, RISK_STRONG = 2 } RiskLevel;

typedef enum {
	ENGINE_OK = 0,
	ERR_NO_FREE_SLOT,
	ERR_CHARACTER_NOT_FOUND,
	ERR_CHARACTER_NOT_ACTIVE,
	ERR_PENDING_LIMIT,
	ERR_DEADLINE_IN_PAST,
	ERR_MISSION_NOT_FOUND,
	ERR_MISSION_NOT_PENDING,
	ERR_NO_SPACE,
} EngineError;

typedef struct {
	uint16_t id;                 // 0 = registro vacío
	char     name[NAME_LEN];
	uint8_t  slotNumber;         // 1..3; 0 = liberado (boda/abandono)
	uint8_t  status;             // CharacterStatus
	uint8_t  level;              // 0..3, INDEPENDIENTE de heartsTotal
	int16_t  heartsTotal;        // único contador, clamp mínimo 0
	GameDate createdDate;
	GameDate lastMissionCompletedDate; // 0 = nunca
	GameDate inactivitySince;    // ancla del reloj de abandono; nunca para
	uint8_t  pendingAbandonmentScene;
	uint8_t  pendingCancellationScene;
	uint8_t  milestonesShown;    // bitmask, reservado para la reactividad (Fase 3)
	uint16_t completedCount;     // total histórico de misiones completadas
	// P12 ranking semanal:
	int16_t  weeklyHearts;       // corazones netos de la semana vigente (puede ser negativo)
	uint16_t weeksWon;           // semanas que quedó #1 (cosmético)
	// P12 creador de personaje (solo cosmético, nunca mecánico):
	uint8_t  appearanceBase, appearancePalette, appearanceAccessory, appearanceExpression;
} Character;

typedef struct {
	uint16_t id;                 // 0 = registro vacío
	uint16_t characterId;
	char     name[NAME_LEN];
	uint8_t  difficulty;         // Difficulty
	uint8_t  status;             // MissionStatus
	GameDate deadline;
	GameDate completedDate;      // 0 = null
	int16_t  heartsAwarded;      // + al completar, - penalización; 0 = null
} Mission;

typedef struct {
	uint16_t id;
	char     characterName[NAME_LEN];
	uint16_t originalCharacterId;
	GameDate weddingDate;
	uint16_t completedCount;
} HappyEnding;

typedef struct {
	Character   characters[MAX_CHARACTER_RECORDS];
	Mission     missions[MAX_MISSIONS];
	HappyEnding happyEndings[MAX_HAPPY_ENDINGS];
	uint8_t     happyEndingCount;
	uint16_t    nextId;          // arranca en 1
	GameDate    weekAnchor;      // lunes de la semana vigente (P12)
	uint16_t    lastWeekWinnerId;// characterId del #1 de la semana cerrada; 0 = nadie
} GameState;

// ---- resultados de mutaciones ----

typedef struct {
	int16_t heartsEarned;
	bool    leveledUp;
	uint8_t newLevel;
	bool    wedding;
} CompleteResult;

typedef struct {
	uint16_t characterId;
	uint8_t  previousLevel;
	uint8_t  newLevel;
	bool     slotFreed;
} AbandonmentEvent;

typedef struct {
	AbandonmentEvent items[MAX_CHARACTER_RECORDS];
	uint8_t          count;
} AbandonmentEvents;

typedef enum { SCENE_ABANDONMENT = 0, SCENE_CANCELLATION = 1 } SceneKind;
typedef struct {
	uint8_t  kind;               // SceneKind
	uint16_t characterId;
	uint16_t missionId;          // solo cancelación
} StartupScene;
typedef struct {
	StartupScene items[MAX_CHARACTER_RECORDS * 2];
	uint8_t      count;
} StartupScenes;

typedef struct {
	bool     closed;             // hubo cambio de semana
	uint16_t winnerCharacterId;  // 0 = nadie ganó (nadie con neto > 0)
} WeekCloseResult;

// ---- init ----
void gameInit(GameState *s, GameDate today);

// ---- fechas / calendario ----
GameDate weekStart(GameDate d);  // lunes de la semana de d

// ---- consultas puras ----
Character *findCharacter(GameState *s, uint16_t id);
Mission   *findMission(GameState *s, uint16_t id);
int        activeCharacterCount(const GameState *s);
int        pendingMissionCount(const GameState *s, uint16_t characterId);
uint8_t    firstFreeSlot(const GameState *s);          // 0 = ninguno
int32_t    daysInactive(const Character *c, GameDate today);
int32_t    daysTogether(const Character *c, GameDate today);
bool       isAtRisk(const Character *c, GameDate today);
RiskLevel  riskLevel(int32_t daysInactiveCount);
int32_t    daysUntilLeaving(int32_t daysInactiveCount);

// ---- corazones / niveles ----
int16_t heartsForDifficulty(Difficulty d);
int16_t cancelPenaltyFor(Difficulty d);
int16_t calcHeartsEarned(Difficulty d, GameDate deadline, GameDate completedAt);
uint8_t levelForHearts(int16_t hearts);
// Barra de progreso con clamp de presentación (fix QA M2). false si nivel máx.
bool heartsToNextLevel(uint8_t level, int16_t hearts, int16_t *current, int16_t *total);

// ---- mutaciones ----
EngineError createCharacter(GameState *s, const char *name, GameDate today, uint16_t *outId);
EngineError deleteCharacter(GameState *s, uint16_t characterId);
EngineError setAppearance(GameState *s, uint16_t characterId,
                          uint8_t base, uint8_t palette, uint8_t accessory, uint8_t expression);
EngineError createMission(GameState *s, uint16_t characterId, const char *name,
                          Difficulty d, GameDate deadline, GameDate today, uint16_t *outId);
EngineError completeMission(GameState *s, uint16_t missionId, GameDate today, CompleteResult *out);
EngineError cancelMission(GameState *s, uint16_t missionId, int16_t *outPenalty);
EngineError acceptMissionLoss(GameState *s, uint16_t missionId, int16_t *outPenalty); // P4
EngineError rescheduleMission(GameState *s, uint16_t missionId, GameDate newDeadline,
                              GameDate today, int16_t *outPenalty, uint16_t *outNewMissionId);
void checkAbandonment(GameState *s, GameDate today, AbandonmentEvents *out);
void acknowledgeAbandonmentScene(GameState *s, uint16_t characterId);
void acknowledgeCancellationScene(GameState *s, uint16_t characterId);

// ---- P12: ranking semanal ----
// Cierra la semana si today ya cayó en una posterior a weekAnchor.
void rankingWeeklyCheck(GameState *s, GameDate today, WeekCloseResult *out);
// Líder actual: activo con weeklyHearts máximo y > 0; desempate determinista
// (más recientemente activo, luego creado antes, luego slot menor). 0 = nadie.
uint16_t rankingLeader(GameState *s);

// ---- arranque (todos los checks de tiempo corren al abrir) ----
// Orden: cierre de semana -> abandono -> escenas (nuevas + re-hidratadas).
void buildStartup(GameState *s, GameDate today, StartupScenes *scenes, WeekCloseResult *week);

#endif
