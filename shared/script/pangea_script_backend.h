#pragma once

#include "pangea_script.h"

typedef struct PangeaScriptBackend PangeaScriptBackend;

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo);
void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend);
/* Optional hooks that have no callable Lua handler do not count as successes. */
bool PangeaScriptBackend_DidExecuteHook(PangeaScriptBackend* backend);
PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_LoadNamed(PangeaScriptBackend* backend, const char* source, const char* sourceName, char* error, int errorCapacity);
void PangeaScriptBackend_SetStaging(PangeaScriptBackend* backend, bool staging);
void PangeaScriptBackend_CopyLastError(PangeaScriptBackend* destination, const PangeaScriptBackend* source);
void PangeaScriptBackend_CopyExecutionContext(PangeaScriptBackend* destination, const PangeaScriptBackend* source);
void PangeaScriptBackend_SetLevelContext(PangeaScriptBackend* backend, int levelNum);
PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTriggerHook(PangeaScriptBackend* backend, const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallPickupHook(PangeaScriptBackend* backend, const PangeaScriptPickupContext* context, PangeaScriptPickupResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallDamageHook(PangeaScriptBackend* backend, const PangeaScriptDamageContext* context, PangeaScriptDamageResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallDamageAppliedHook(PangeaScriptBackend* backend, const PangeaScriptDamageContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallPlayerEvent(PangeaScriptBackend* backend, const PangeaScriptPlayerEventContext* context, const char* event, char* error, int errorCapacity);
void PangeaScriptBackend_ClearObjectState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle);
void PangeaScriptBackend_ClearObjectResources(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle);
void PangeaScriptBackend_CaptureObjectCheckpointState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle);
bool PangeaScriptBackend_RestoreObjectCheckpointState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle);
void PangeaScriptBackend_ResetObjectStates(PangeaScriptBackend* backend);
