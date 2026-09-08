#pragma once

/*
 * The descriptor lists are generated from the validated frontend scripting
 * contract. The runtime tables and contract checker consume this include.
 */

#define PANGEA_SCRIPT_OTTO_MATIC_HOOK_LIST(X) \
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
	X("onPlayerSpawn") \
	X("onCheckpointReached") \
	X("onPlayerRespawn") \
	X("onDeath") \
	X("onSave") \
	X("onLoad")

#define PANGEA_SCRIPT_BUGDOM_HOOK_LIST(X) \
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
	X("onPlayerSpawn") \
	X("onCheckpointReached") \
	X("onObjectiveComplete") \
	X("onPlayerRespawn") \
	X("onDeath") \
	X("onSave") \
	X("onLoad")

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
	X("onCheckpointReached") \
	X("onObjectiveComplete") \
	X("onPlayerRespawn") \
	X("onSave") \
	X("onLoad")

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
	X("onWeaponHit") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onPlayerRespawn") \
	X("onDeath")

#define PANGEA_SCRIPT_NANOSAUR2_HOOK_LIST(X) \
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
	X("onPlayerSpawn") \
	X("onCheckpointReached") \
	X("onLapComplete") \
	X("onRaceFinish") \
	X("onObjectiveComplete") \
	X("onPlayerRespawn") \
	X("onDeath") \
	X("onSave") \
	X("onLoad")

#define PANGEA_SCRIPT_CRO_MAG_RALLY_HOOK_LIST(X) \
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
	X("onCheckpointReached") \
	X("onLapComplete") \
	X("onRaceFinish") \
	X("onDeath")

#define PANGEA_SCRIPT_BILLY_FRONTIER_HOOK_LIST(X) \
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
	X("onDeath") \
	X("onSave") \
	X("onLoad")

#define PANGEA_SCRIPT_MIGHTY_MIKE_HOOK_LIST(X) \
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
	X("onTriggerEnter") \
	X("onWeaponHit") \
	X("onDamage") \
	X("onDamageApplied") \
	X("onPlayerSpawn") \
	X("onPlayerRespawn") \
	X("onDeath") \
	X("onSave") \
	X("onLoad")

#define PANGEA_SCRIPT_OTTO_MATIC_CAPABILITIES { true, true, false, true, true, true, true, true, true, false }

#define PANGEA_SCRIPT_BUGDOM_CAPABILITIES { true, true, false, true, true, true, true, true, true, true }

#define PANGEA_SCRIPT_BUGDOM2_CAPABILITIES { true, true, false, true, true, true, true, true, true, false }

#define PANGEA_SCRIPT_NANOSAUR_CAPABILITIES { true, false, false, true, true, true, true, true, true, false }

#define PANGEA_SCRIPT_NANOSAUR2_CAPABILITIES { true, true, false, false, true, false, true, true, false, false }

#define PANGEA_SCRIPT_CRO_MAG_RALLY_CAPABILITIES { true, true, false, false, true, false, false, true, false, false }

#define PANGEA_SCRIPT_BILLY_FRONTIER_CAPABILITIES { true, true, false, true, true, true, true, true, true, false }

#define PANGEA_SCRIPT_MIGHTY_MIKE_CAPABILITIES { false, false, true, true, true, true, true, true, true, false }

#define PANGEA_SCRIPT_COMMAND_DESCRIPTOR_LIST(X) \
	X("pangea.player.setHealth", "player-health", "disabled-network", "callback", "integer player index; finite health in range 0..1; adapter health mutation support") \
	X("pangea.player.setLives", "player-lives", "disabled-network", "callback", "integer player index; non-negative integer lives count; adapter lives mutation support") \
	X("pangea.player.setScore", "player-score", "disabled-network", "callback", "integer player index; non-negative integer score in range 0..UINT32_MAX; adapter score mutation support") \
	X("pangea.player.setWeaponQuantity", "player-inventory", "disabled-network", "callback", "integer player index; valid adapter weapon type; non-negative quantity in range 0..999; adapter inventory mutation support") \
	X("pangea.player.setKey", "player-keys", "disabled-network", "callback", "integer player index; valid adapter key identifier; boolean enabled state; adapter key inventory mutation support") \
	X("pangea.player.setCloverCount", "player-collectibles", "disabled-network", "callback", "integer player index; named clover color green, blue, or gold; non-negative count in range 0..999; adapter collectible mutation support") \
	X("pangea.player.setShieldActive", "player-shield", "disabled-network", "callback", "integer player index; boolean active state; adapter-native shield activation/deactivation support") \
	X("pangea.player.heal", "player-heal", "disabled-network", "callback", "integer player index; finite non-negative health amount in range 0..1; adapter health read and mutation support") \
	X("pangea.player.setInvulnerable", "player-invulnerability", "disabled-network", "callback", "integer player index; finite duration in range 0..3600 seconds; adapter invulnerability timer support") \
	X("pangea.player.setPosition", "player-position", "disabled-network", "callback", "integer player index; finite Vector3; adapter player-position mutation support") \
	X("pangea.player.setVelocity", "player-velocity", "disabled-network", "callback", "integer player index; finite Vector3; adapter player-velocity mutation support") \
	X("pangea.player.setForm", "player-form", "disabled-network", "callback", "player index; supported native form identifier such as bug or ball") \
	X("pangea.object.setPosition", "object-position", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setPositionOffset", "object-position-offset", "disabled-network", "callback", "generation-checked handle; finite Vector3; only during onObjectFrame") \
	X("pangea.object.setVelocity", "object-velocity", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setRotation", "object-rotation", "disabled-network", "callback", "generation-checked handle; finite Vector3") \
	X("pangea.object.setScale", "object-scale", "disabled-network", "callback", "generation-checked handle; finite scale in range 0.000001..100") \
	X("pangea.object.setAnimation", "object-animation", "disabled-network", "callback", "generation-checked handle; finite speed and blend seconds; declared animation index or name") \
	X("pangea.object.setCollisionEnabled", "object-collision", "disabled-network", "callback", "generation-checked handle; boolean enabled state; adapter collision toggle support") \
	X("pangea.object.setActive", "object-activation", "disabled-network", "callback", "generation-checked handle; enabled-state transition") \
	X("pangea.object.delete", "object-delete", "disabled-network", "callback", "generation-checked handle; delete capability")

#define PANGEA_SCRIPT_EVENT_DESCRIPTOR_LIST(X) \
	X("onTriggerEnter", "callback", "self:ObjectHandle;other:ObjectHandle|nil;sideBits:integer", "TriggerResult|nil") \
	X("onPickupCollected", "next-engine-phase", "pickup:ObjectHandle;player:ObjectHandle|nil;position:Vector3", "PickupResult|nil") \
	X("onWeaponHit", "next-engine-phase", "weapon:ObjectHandle|nil;target:ObjectHandle|nil;damage:number", "WeaponHitResult|nil") \
	X("onDamage", "callback", "target:ObjectHandle;source:ObjectHandle|nil;damage:number;cause:integer", "DamageResult|nil") \
	X("onDamageApplied", "callback", "target:ObjectHandle;source:ObjectHandle|nil;damage:number;cause:integer", "nil") \
	X("onDeath", "callback", "player:ObjectHandle;eventValue:integer;velocity:Vector3|nil", "nil") \
	X("onPlayerSpawn", "callback", "player:ObjectHandle;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("onPlayerRespawn", "callback", "player:ObjectHandle;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("onCheckpointReached", "callback", "player:ObjectHandle;eventValue:integer;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("onLapComplete", "callback", "player:ObjectHandle;eventValue:integer;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("onRaceFinish", "callback", "player:ObjectHandle;eventValue:integer;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("onObjectiveComplete", "callback", "player:ObjectHandle;eventValue:integer;position:Vector3;velocity:Vector3|nil;collisionEnabled:boolean|nil;rotation:Vector3|nil", "nil") \
	X("animationComplete", "callback", "object:ObjectHandle", "nil") \
	X("destroy", "callback", "object:ObjectHandle", "nil")

#define PANGEA_SCRIPT_OBJECT_EVENT_DESCRIPTOR_LIST(X) \
	X("spawn", "onSpawn", "callback", "none", "preserve", false) \
	X("update", "onUpdate", "callback", "none", "preserve", false) \
	X("triggerEnter", "onTriggerEnter", "callback", "none", "preserve", false) \
	X("triggerStay", "onTriggerStay", "callback", "none", "preserve", false) \
	X("triggerExit", "onTriggerExit", "callback", "none", "preserve", false) \
	X("animationEvent", "onAnimationEvent", "callback", "none", "preserve", false) \
	X("animationComplete", "onAnimationComplete", "callback", "none", "preserve", false) \
	X("activate", "onActivate", "callback", "none", "preserve", false) \
	X("deactivate", "onDeactivate", "callback", "owner-resources", "clear", false) \
	X("streamIn", "onStreamIn", "callback", "none", "preserve", false) \
	X("streamOut", "onStreamOut", "callback", "owner-resources", "clear", true) \
	X("checkpointReset", "onCheckpointReset", "callback", "owner-resources", "preserve", false) \
	X("destroy", "onDestroy", "callback", "owner-resources", "clear", true)
