#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PANGEA_PRESENTATION_TEXT 512
#define PANGEA_PRESENTATION_ID 64
#define PANGEA_PRESENTATION_ROWS 12
#define PANGEA_PRESENTATION_CHOICES 4
#define PANGEA_PRESENTATION_NODES 16
#define PANGEA_PRESENTATION_PLAYERS 4

typedef struct PangeaScriptObjectHandle PangeaScriptObjectHandle;

typedef enum PangeaPresentationStatus
{
	PANGEA_PRESENTATION_OK,
	PANGEA_PRESENTATION_BAD_ARGUMENT,
	PANGEA_PRESENTATION_UNSUPPORTED,
	PANGEA_PRESENTATION_ASSET_NOT_LOADED,
	PANGEA_PRESENTATION_CAPACITY,
	PANGEA_PRESENTATION_STALE_HANDLE,
} PangeaPresentationStatus;

typedef struct PangeaPresentationHandle
{
	uint32_t id;
	uint32_t generation;
} PangeaPresentationHandle;

typedef struct PangeaPresentationOptions
{
	float duration;
	float volume;
	float pitch;
	float scale;
	int player;
	int priority;
	bool positional;
	bool loop;
	bool modal;
	bool hasOwner;
	int ownerID;
	uint32_t ownerGeneration;
	float x, y, z;
} PangeaPresentationOptions;

typedef struct PangeaPresentationRow
{
	char text[128];
	int player;
	int priority;
	bool dialogue;
} PangeaPresentationRow;

typedef struct PangeaPresentationNative
{
	bool positionalAudio;
	bool attachedEffects;
	bool text;
	bool choices;
	int players;
	int (*activePlayers)(void);
	bool (*textAvailable)(void);
	bool (*beginDialogue)(void);
	void (*endDialogue)(void);
	void (*input)(void);
	void (*reset)(void);
	PangeaPresentationStatus (*play)(const char* name, const PangeaPresentationOptions* options, int* voice);
	void (*stop)(int voice);
	bool (*playing)(int voice);
	void (*position)(int voice, float x, float y, float z);
	PangeaPresentationStatus (*burst)(const char* name, const PangeaPresentationOptions* options);
	const char* const* sounds;
	int soundCount;
	const char* const* effects;
	int effectCount;
} PangeaPresentationNative;

typedef struct PangeaDialogueChoice
{
	char id[PANGEA_PRESENTATION_ID];
	char text[128];
	int next;
} PangeaDialogueChoice;

typedef struct PangeaDialogueNode
{
	char speaker[PANGEA_PRESENTATION_ID];
	char text[PANGEA_PRESENTATION_TEXT];
	char voice[PANGEA_PRESENTATION_ID];
	float duration;
	int next;
	int choiceCount;
	PangeaDialogueChoice choices[PANGEA_PRESENTATION_CHOICES];
} PangeaDialogueNode;

typedef struct PangeaDialogueDefinition
{
	char id[PANGEA_PRESENTATION_ID];
	int nodeCount;
	PangeaDialogueNode nodes[PANGEA_PRESENTATION_NODES];
} PangeaDialogueDefinition;

typedef struct PangeaDialogueEvent
{
	PangeaPresentationHandle handle;
	int player;
	char id[PANGEA_PRESENTATION_ID];
	char choice[PANGEA_PRESENTATION_ID];
	char reason[32];
} PangeaDialogueEvent;

void PangeaPresentation_Configure(const PangeaPresentationNative* native);
const PangeaPresentationNative* PangeaPresentation_GetNative(void);
PangeaPresentationOptions PangeaPresentation_DefaultOptions(void);
bool PangeaPresentation_OptionsValid(const PangeaPresentationOptions* options);
PangeaPresentationStatus PangeaPresentation_Message(const char* text, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle);
PangeaPresentationStatus PangeaPresentation_Objective(const char* id, const char* text, int current, int total, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle);
PangeaPresentationStatus PangeaPresentation_Play(const char* id, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle);
PangeaPresentationStatus PangeaPresentation_Effect(const char* id, const PangeaPresentationOptions* options, bool attached, PangeaPresentationHandle* handle);
PangeaPresentationStatus PangeaPresentation_Remove(PangeaPresentationHandle handle);
bool PangeaPresentation_Active(PangeaPresentationHandle handle);
void PangeaPresentation_Update(float delta);
void PangeaPresentation_Reset(void);
void PangeaPresentation_ClearOwner(int id, uint32_t generation);
int PangeaPresentation_Rows(PangeaPresentationRow* rows, int capacity);
int PangeaPresentation_RowsForPlayer(PangeaPresentationRow* rows, int capacity, int player);

PangeaPresentationStatus PangeaDialogue_Define(const PangeaDialogueDefinition* definition);
PangeaPresentationStatus PangeaDialogue_Start(const char* id, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle);
PangeaPresentationStatus PangeaDialogue_Cancel(PangeaPresentationHandle handle);
bool PangeaDialogue_Active(PangeaPresentationHandle handle);
void PangeaDialogue_Update(float delta);
void PangeaDialogue_Input(bool confirm, bool back, int direction);
void PangeaDialogue_InputForPlayer(int player, bool confirm, bool back, int direction);
bool PangeaDialogue_IsActiveForPlayer(int player);
void PangeaDialogue_Reset(void);
void PangeaDialogue_ClearOwner(int id, uint32_t generation);
int PangeaDialogue_Rows(PangeaPresentationRow* rows, int capacity);
int PangeaDialogue_RowsForPlayer(PangeaPresentationRow* rows, int capacity, int player);
bool PangeaDialogue_Poll(PangeaDialogueEvent* event);
int PangeaDialogue_Player(void);
void PangeaDialogue_ClearDefinitions(void);
