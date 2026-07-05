#pragma once

#include "pangea_script.h"

typedef struct PangeaScriptBackend PangeaScriptBackend;

PangeaScriptBackend* PangeaScriptBackend_Create(const PangeaScriptGameInfo* gameInfo);
void PangeaScriptBackend_Destroy(PangeaScriptBackend* backend);
PangeaScriptStatus PangeaScriptBackend_Load(PangeaScriptBackend* backend, const char* source, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallLevelHook(PangeaScriptBackend* backend, PangeaScriptHook hook, const PangeaScriptLevelContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallNamedLevelHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptLevelContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallFrameHook(PangeaScriptBackend* backend, const PangeaScriptFrameContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallNamedFrameHook(PangeaScriptBackend* backend, const char* hookName, const PangeaScriptFrameContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTerrainItemHook(PangeaScriptBackend* backend, PangeaScriptTerrainItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallSplineItemHook(PangeaScriptBackend* backend, PangeaScriptSplineItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallMapItemHook(PangeaScriptBackend* backend, PangeaScriptMapItemContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallPickupCollectedHook(PangeaScriptBackend* backend, PangeaScriptPickupContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallWeaponHitHook(PangeaScriptBackend* backend, PangeaScriptWeaponHitContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallTriggerEnterHook(PangeaScriptBackend* backend, PangeaScriptTriggerContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectCollisionHook(PangeaScriptBackend* backend, PangeaScriptObjectCollisionContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallPlayerDamageHook(PangeaScriptBackend* backend, PangeaScriptPlayerDamageContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectDamageHook(PangeaScriptBackend* backend, PangeaScriptObjectDamageContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectDeleteHook(PangeaScriptBackend* backend, const PangeaScriptObjectDeleteContext* context, char* error, int errorCapacity);
PangeaScriptStatus PangeaScriptBackend_CallObjectFrameHook(PangeaScriptBackend* backend, const PangeaScriptObjectFrameContext* context, PangeaScriptObjectFrameResult* result, char* error, int errorCapacity);
