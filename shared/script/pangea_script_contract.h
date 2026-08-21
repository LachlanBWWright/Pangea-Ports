#pragma once

/*
 * The descriptor lists are intentionally kept in a C-compatible include so
 * the runtime tables and contract checker consume the same metadata.
 */

#define PANGEA_SCRIPT_LEVEL_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onLevelLoad") \
	X("onLevelStart") \
	X("onFrame") \
	X("onLevelComplete") \
	X("onLevelUnload") \
	X("onTerrainItem") \
	X("onSplineItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onPlayerRespawn") \
	X("onDeath")

#define PANGEA_SCRIPT_BUGDOM2_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onLevelLoad") \
	X("onLevelStart") \
	X("onFrame") \
	X("onLevelComplete") \
	X("onLevelUnload") \
	X("onTerrainItem") \
	X("onSplineItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter") \
	X("onWeaponHit") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onDeath") \
	X("onPlayerSpawn") \
	X("onPlayerRespawn")

#define PANGEA_SCRIPT_RACE_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onRaceLoad") \
	X("onRaceStart") \
	X("onRaceFrame") \
	X("onRaceComplete") \
	X("onRaceUnload") \
	X("onTerrainItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onDeath")

#define PANGEA_SCRIPT_AREA_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onAreaLoad") \
	X("onAreaStart") \
	X("onAreaFrame") \
	X("onAreaComplete") \
	X("onAreaUnload") \
	X("onTerrainItem") \
	X("onSplineItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onDeath")

#define PANGEA_SCRIPT_MAP_AREA_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onAreaLoad") \
	X("onAreaStart") \
	X("onAreaFrame") \
	X("onAreaComplete") \
	X("onAreaUnload") \
	X("onMapItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter")

#define PANGEA_SCRIPT_NANOSAUR_HOOK_LIST(X) \
	X("onGameStart") \
	X("onGameShutdown") \
	X("onLevelLoad") \
	X("onLevelStart") \
	X("onFrame") \
	X("onLevelComplete") \
	X("onLevelUnload") \
	X("onTerrainItem") \
	X("onObjectFrame") \
	X("onPickupCollected") \
	X("onTriggerEnter") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onPlayerRespawn") \
	X("onDeath")

#define PANGEA_SCRIPT_COMMAND_DESCRIPTOR_LIST(X) \
	X("pangea.object.setPosition", "object-position", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setVelocity", "object-velocity", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setRotation", "object-rotation", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setScale", "object-scale", "disabled-network", "callback", "generation-checked handle; finite scale in range 0.000001..100") \
	X("pangea.object.setAnimation", "object-animation", "disabled-network", "callback", "generation-checked handle; finite speed and blend seconds; declared animation index or name") \
	X("pangea.object.setActive", "object-activation", "disabled-network", "callback", "generation-checked handle; enabled-state transition") \
	X("pangea.object.delete", "object-delete", "disabled-network", "callback", "generation-checked handle; delete capability")

#define PANGEA_SCRIPT_EVENT_DESCRIPTOR_LIST(X) \
	X("onTriggerEnter", "callback", "self:ObjectHandle;other:ObjectHandle|nil;sideBits:integer", "TriggerResult|nil") \
	X("onPickupCollected", "next-engine-phase", "pickup:ObjectHandle;player:ObjectHandle|nil;position:Vector3", "PickupResult|nil") \
	X("onWeaponHit", "next-engine-phase", "weapon:ObjectHandle|nil;target:ObjectHandle|nil;damage:number", "WeaponHitResult|nil") \
	X("onDamage", "callback", "target:ObjectHandle;source:ObjectHandle|nil;damage:number;cause:integer", "DamageResult|nil") \
	X("onDamageApplied", "callback", "target:ObjectHandle;source:ObjectHandle|nil;damage:number;cause:integer", "nil") \
	X("onDeath", "callback", "player:ObjectHandle;eventValue:integer", "nil") \
	X("onPlayerSpawn", "callback", "player:ObjectHandle;position:Vector3", "nil") \
	X("onPlayerRespawn", "callback", "player:ObjectHandle;position:Vector3", "nil") \
	X("animationComplete", "callback", "object:ObjectHandle", "nil") \
	X("destroy", "callback", "object:ObjectHandle", "nil")

#define PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_LIST(X) \
	X("spawn", "onSpawn", "callback", "none", "preserve", false) \
	X("update", "onUpdate", "callback", "none", "preserve", false) \
	X("triggerEnter", "onTriggerEnter", "callback", "none", "preserve", false) \
	X("triggerStay", "onTriggerStay", "callback", "none", "preserve", false) \
	X("animationEvent", "onAnimationEvent", "callback", "none", "preserve", false) \
	X("animationComplete", "onAnimationComplete", "callback", "none", "preserve", false) \
	X("activate", "onActivate", "callback", "none", "preserve", false) \
	X("deactivate", "onDeactivate", "callback", "owner-resources", "clear", false) \
	X("streamIn", "onStreamIn", "callback", "none", "preserve", false) \
	X("streamOut", "onStreamOut", "callback", "owner-resources", "clear", true) \
	X("checkpointReset", "onCheckpointReset", "callback", "owner-resources", "preserve", false) \
	X("destroy", "onDestroy", "callback", "owner-resources", "clear", true)
