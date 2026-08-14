#pragma once

#include "pangea_script.h"

typedef struct PangeaScriptBackend PangeaScriptBackend;

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo);
void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend);
PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTriggerHook(PangeaScriptBackend* backend, const PangeaScriptTriggerContext* context, PangeaScriptTriggerResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallPickupHook(PangeaScriptBackend* backend, const PangeaScriptPickupContext* context, PangeaScriptPickupResult* result, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, const PangeaScriptWeaponHitContext* context, PangeaScriptWeaponHitResult* result, char* error, int errorCapacity);
void PangeaScriptBackend_ClearObjectState(PangeaScriptBackend* backend, PangeaScriptObjectHandle handle);
void PangeaScriptBackend_ResetObjectStates(PangeaScriptBackend* backend);
