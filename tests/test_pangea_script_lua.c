#include "../shared/script/pangea_script.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct DummyObject
{
	PangeaScriptVector3 position;
	PangeaScriptVector3 velocity;
	int type;
	int kind;
	int mode;
	unsigned int statusBits;
	unsigned int cType;
	unsigned int cBits;
	float health;
	float damage;
	PangeaScriptObjectBounds bounds;
	PangeaScriptObjectParams params;
	bool deleted;
} DummyObject;

static bool DummyGetPosition(void* nativeObject, PangeaScriptVector3* outPosition)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	*outPosition = obj->position;
	return true;
}

static bool DummySetPosition(void* nativeObject, const PangeaScriptVector3* position)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->position = *position;
	return true;
}

static bool DummySetVelocity(void* nativeObject, const PangeaScriptVector3* velocity)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->velocity = *velocity;
	return true;
}

static bool DummyDeleteObject(void* nativeObject)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->deleted = true;
	return true;
}

static bool DummyGetInfo(void* nativeObject, PangeaScriptObjectInfo* outInfo)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	*outInfo = (PangeaScriptObjectInfo)
	{
		.type = obj->type,
		.kind = obj->kind,
		.mode = obj->mode,
		.statusBits = obj->statusBits,
		.cType = obj->cType,
		.cBits = obj->cBits,
		.health = obj->health,
		.damage = obj->damage,
		.velocity = obj->velocity,
		.validMask = PANGEA_SCRIPT_OBJECT_INFO_TYPE
			| PANGEA_SCRIPT_OBJECT_INFO_KIND
			| PANGEA_SCRIPT_OBJECT_INFO_MODE
			| PANGEA_SCRIPT_OBJECT_INFO_FLAGS
			| PANGEA_SCRIPT_OBJECT_INFO_COLLISION
			| PANGEA_SCRIPT_OBJECT_INFO_HEALTH
			| PANGEA_SCRIPT_OBJECT_INFO_DAMAGE
			| PANGEA_SCRIPT_OBJECT_INFO_VELOCITY,
	};
	return true;
}

static bool DummySetInfo(void* nativeObject, const PangeaScriptObjectInfo* info)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_TYPE)
		obj->type = info->type;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_KIND)
		obj->kind = info->kind;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_MODE)
		obj->mode = info->mode;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_FLAGS)
		obj->statusBits = info->statusBits;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_COLLISION)
	{
		obj->cType = info->cType;
		obj->cBits = info->cBits;
	}
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_HEALTH)
		obj->health = info->health;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_DAMAGE)
		obj->damage = info->damage;
	if (info->validMask & PANGEA_SCRIPT_OBJECT_INFO_VELOCITY)
		obj->velocity = info->velocity;
	return true;
}

static bool DummyGetBounds(void* nativeObject, PangeaScriptObjectBounds* outBounds)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	*outBounds = obj->bounds;
	return true;
}

static bool DummySetBounds(void* nativeObject, const PangeaScriptObjectBounds* bounds)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->bounds = *bounds;
	return true;
}

static bool DummyGetParams(void* nativeObject, PangeaScriptObjectParams* outParams)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	*outParams = obj->params;
	return true;
}

static bool DummySetParams(void* nativeObject, const PangeaScriptObjectParams* params)
{
	DummyObject* obj = (DummyObject*) nativeObject;
	obj->params = *params;
	return true;
}

static const PangeaScriptObjectOps kDummyOps = {
	.getPosition = DummyGetPosition,
	.setPosition = DummySetPosition,
	.setVelocity = DummySetVelocity,
	.deleteObject = DummyDeleteObject,
	.getInfo = DummyGetInfo,
	.setInfo = DummySetInfo,
	.getBounds = DummyGetBounds,
	.setBounds = DummySetBounds,
	.getParams = DummyGetParams,
	.setParams = DummySetParams,
};

static DummyObject gSpawnedNativeDummy;
static DummyObject gDeferredDeleteDummy;
static PangeaScriptPlayerInfo gDummyPlayerInfo;
static PangeaScriptSoundRequest gLastSoundRequest;
static PangeaScriptEffectRequest gLastEffectRequest;
static int gSoundRequestCount;
static int gEffectRequestCount;

static PangeaScriptStatus TestSpawnNativeCallback(const char* id, float x, float y, float z, int subtype, int amount, PangeaScriptObjectHandle* outHandle)
{
	(void) subtype;
	(void) amount;
	if (strcmp(id, "test.native") != 0 && strcmp(id, "test.deferred-delete") != 0)
		return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;

	DummyObject* object = strcmp(id, "test.deferred-delete") == 0 ? &gDeferredDeleteDummy : &gSpawnedNativeDummy;
	object->position.x = x;
	object->position.y = y;
	object->position.z = z;
	object->type = 7;
	object->kind = 8;
	object->mode = 9;
	object->statusBits = 10;
	object->cType = 11;
	object->cBits = 12;
	object->health = 1.0f;
	object->damage = 0.5f;
	object->bounds = (PangeaScriptObjectBounds){ -1.0f, 1.0f, -2.0f, 2.0f, 3.0f, -3.0f };
	object->params = (PangeaScriptObjectParams){ .values = { 10, 20, 30, 40 }, .count = 4 };
	object->deleted = false;

	static const char* tags[] = { "pickup", "test" };
	PangeaScriptObjectRegistration reg = {
		.nativeObject = object,
		.ops = &kDummyOps,
		.tags = tags,
		.tagCount = 2,
		.capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL
	};
	return PangeaScript_RegisterObject(&reg, outHandle);
}

static bool TestGetPlayerInfo(int playerNum, PangeaScriptPlayerInfo* outInfo)
{
	if (playerNum != 0 || !outInfo)
		return false;

	*outInfo = gDummyPlayerInfo;
	outInfo->playerNum = playerNum;
	return true;
}

static bool TestSetPlayerInfo(int playerNum, const PangeaScriptPlayerInfo* info)
{
	if (playerNum != 0 || !info)
		return false;

	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SCORE)
		gDummyPlayerInfo.score = info->score;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_HEALTH)
		gDummyPlayerInfo.health = info->health;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_LIVES)
		gDummyPlayerInfo.lives = info->lives;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_AMMO)
		gDummyPlayerInfo.ammo = info->ammo;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_FUEL)
		gDummyPlayerInfo.fuel = info->fuel;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_SHIELD)
		gDummyPlayerInfo.shield = info->shield;
	if (info->validMask & PANGEA_SCRIPT_PLAYER_INFO_CURRENCY)
		gDummyPlayerInfo.currency = info->currency;
	gDummyPlayerInfo.validMask |= info->validMask;
	return true;
}

static bool TestPlaySound(const PangeaScriptSoundRequest* request)
{
	if (!request)
		return false;

	gLastSoundRequest = *request;
	gSoundRequestCount++;
	return true;
}

static bool TestSpawnEffect(const PangeaScriptEffectRequest* request)
{
	if (!request)
		return false;

	gLastEffectRequest = *request;
	gEffectRequestCount++;
	return true;
}

static void write_temp_file(const char* path, const char* content)
{
	FILE* f = fopen(path, "w");
	assert(f != NULL);
	fputs(content, f);
	fclose(f);
}

static void test_lua_integration(void)
{
	printf("Running Lua backend integration tests...\n");

	PangeaScriptGameInfo gameInfo = {
		.gameId = "TestGame",
		.gameName = "Test Game",
		.spawnNative = TestSpawnNativeCallback,
		.getPlayerInfo = TestGetPlayerInfo,
		.setPlayerInfo = TestSetPlayerInfo,
		.playSound = TestPlaySound,
		.spawnEffect = TestSpawnEffect
	};
	gDummyPlayerInfo = (PangeaScriptPlayerInfo)
	{
		.playerNum = 0,
		.score = 100,
		.health = 0.5f,
		.lives = 3,
		.ammo = 4,
		.fuel = 0.75f,
		.shield = 0.25f,
		.currency = 9,
		.validMask = PANGEA_SCRIPT_PLAYER_INFO_SCORE
			| PANGEA_SCRIPT_PLAYER_INFO_HEALTH
			| PANGEA_SCRIPT_PLAYER_INFO_LIVES
			| PANGEA_SCRIPT_PLAYER_INFO_AMMO
			| PANGEA_SCRIPT_PLAYER_INFO_FUEL
			| PANGEA_SCRIPT_PLAYER_INFO_SHIELD
			| PANGEA_SCRIPT_PLAYER_INFO_CURRENCY,
	};
	PangeaScriptStatus status = PangeaScript_Init(&gameInfo);
	assert(status == PANGEA_SCRIPT_OK);

	system("mkdir -p Data/Scripts/config");
	system("mkdir -p Data/Scripts/dist");
	system("mkdir -p Data/Scripts/src/helpers");

	write_temp_file(
		"Data/Scripts/src/helpers/state.lua",
		"local state = {}\n"
		"state.nativeHandle = nil\n"
		"state.scriptedHandle = nil\n"
		"return state\n");

	const char* testScript =
		"local pangea = require('pangea')\n"
		"local state = require('./helpers/state.lua')\n"
		"local module = {}\n"
		"\n"
		"function module.onLevelLoad(ctx)\n"
		"  pangea.log.info('onLevelLoad triggered in Lua')\n"
		"  state.nativeHandle = pangea.spawn.native('test.native', { x = 100, y = 200, z = 300 }, { subtype = 3, amount = 1 })\n"
		"  state.scriptedHandle = pangea.spawn.scripted('my.scripted', { x = 10, y = 20, z = 30 })\n"
		"end\n"
		"\n"
		"function module.onLevelStart(ctx)\n"
		"  pangea.log.info('onLevelStart triggered')\n"
		"  local player = pangea.player.info(0)\n"
		"  assert(player.score == 100)\n"
		"  assert(player.health == 0.5)\n"
		"  assert(player.lives == 3)\n"
		"  assert(player.ammo == 4)\n"
		"  assert(player.fuel == 0.75)\n"
		"  assert(player.shield == 0.25)\n"
		"  assert(player.currency == 9)\n"
		"  assert(pangea.player.setInfo(0, { score = 125, health = 0.9, lives = 5, ammo = 8, fuel = 0.5, shield = 1.0, currency = 12 }))\n"
		"  assert(pangea.effects.playSound(17, { position = { x = 1, y = 2, z = 3 }, volume = 0.5, rate = 1.25 }))\n"
		"  assert(pangea.effects.spawn(23, { position = { x = 4, y = 5, z = 6 }, velocity = { x = 7, y = 8, z = 9 }, scale = 1.5, quantity = 12 }))\n"
		"  local deferredDeleteHandle = pangea.spawn.native('test.deferred-delete', { x = 0, y = 0, z = 0 })\n"
		"  assert(deferredDeleteHandle ~= nil)\n"
		"  assert(pangea.object.requestDelete(deferredDeleteHandle))\n"
		"  assert(pangea.object.exists(deferredDeleteHandle))\n"
		"  if state.nativeHandle then\n"
		"    assert(pangea.object.exists(state.nativeHandle))\n"
		"    assert(pangea.object.hasTag(state.nativeHandle, 'pickup'))\n"
		"    local tags = pangea.object.tags(state.nativeHandle)\n"
		"    assert(#tags == 2)\n"
		"    assert(pangea.object.addTag(state.nativeHandle, 'script.reward'))\n"
		"    assert(pangea.object.addTag(state.nativeHandle, 'script.reward'))\n"
		"    assert(pangea.object.hasTag(state.nativeHandle, 'script.reward'))\n"
		"    tags = pangea.object.tags(state.nativeHandle)\n"
		"    assert(#tags == 3)\n"
		"    assert(pangea.object.removeTag(state.nativeHandle, 'script.reward'))\n"
		"    assert(not pangea.object.hasTag(state.nativeHandle, 'script.reward'))\n"
		"    assert(not pangea.object.removeTag(state.nativeHandle, 'pickup'))\n"
		"    local objectState = pangea.object.state(state.nativeHandle)\n"
		"    objectState.updates = (objectState.updates or 0) + 1\n"
		"    pangea.object.setPosition(state.nativeHandle, { x = 105, y = 205, z = 305 })\n"
		"    pangea.object.setVelocity(state.nativeHandle, { x = 1, y = 2, z = 3 })\n"
		"    local info = pangea.object.info(state.nativeHandle)\n"
		"    assert(info.type == 7)\n"
		"    assert(info.kind == 8)\n"
		"    assert(info.mode == 9)\n"
		"    assert(info.statusBits == 10)\n"
		"    assert(info.cType == 11)\n"
		"    assert(info.cBits == 12)\n"
		"    assert(info.health == 1.0)\n"
		"    assert(info.damage == 0.5)\n"
		"    assert(info.velocity.x == 1)\n"
		"    assert(info.velocity.y == 2)\n"
		"    assert(info.velocity.z == 3)\n"
		"    local bounds = pangea.object.bounds(state.nativeHandle)\n"
		"    assert(bounds.left == -1)\n"
		"    assert(bounds.right == 1)\n"
		"    assert(bounds.front == -2)\n"
		"    assert(bounds.back == 2)\n"
		"    assert(bounds.top == 3)\n"
		"    assert(bounds.bottom == -3)\n"
		"    local params = pangea.object.params(state.nativeHandle)\n"
		"    assert(#params == 4)\n"
		"    assert(params[1] == 10)\n"
		"    assert(params[2] == 20)\n"
		"    assert(params[3] == 30)\n"
		"    assert(params[4] == 40)\n"
		"    assert(pangea.object.setParams(state.nativeHandle, { 11, 22, 33, 44 }))\n"
		"    assert(pangea.object.setBounds(state.nativeHandle, { left = -4, right = 4, front = -5, back = 5, top = 6, bottom = -6 }))\n"
		"    assert(pangea.object.setInfo(state.nativeHandle, { health = 0.75, damage = 0.25, cType = 13, cBits = 14 }))\n"
		"  end\n"
		"  if state.scriptedHandle then\n"
		"    assert(pangea.object.hasTag(state.scriptedHandle, 'scripted'))\n"
		"    assert(pangea.object.hasTag(state.scriptedHandle, 'my.scripted'))\n"
		"    assert(pangea.object.addTag(state.scriptedHandle, 'script.projectile'))\n"
		"    assert(pangea.object.hasTag(state.scriptedHandle, 'script.projectile'))\n"
		"    local pos = pangea.object.position(state.scriptedHandle)\n"
		"    pangea.object.setPosition(state.scriptedHandle, { x = pos.x + 5, y = pos.y + 5, z = pos.z + 5 })\n"
		"    pangea.object.setVelocity(state.scriptedHandle, { x = 9, y = 8, z = 7 })\n"
		"    assert(pangea.object.setParams(state.scriptedHandle, { 55, 66 }))\n"
		"    local scriptedParams = pangea.object.params(state.scriptedHandle)\n"
		"    assert(#scriptedParams == 2)\n"
		"    assert(scriptedParams[1] == 55)\n"
		"    assert(scriptedParams[2] == 66)\n"
		"  end\n"
		"end\n"
		"\n"
		"function module.onTerrainItem(item)\n"
		"  if item.itemType == 42 then\n"
		"    return { handled = true, markInUse = true }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onSplineItem(item)\n"
		"  if item.itemType == 100 then\n"
		"    return { handled = true, markInUse = true }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onMapItem(item)\n"
		"  if item.sceneNum == 5 then\n"
		"    return { handled = true, markInUse = true }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onPickupCollected(ctx)\n"
		"  if ctx.pickupId == 'test.pickup' and ctx.pickupType == 7 then\n"
		"    return { handled = true, consumePickup = true, scoreDelta = 250, healthDelta = 0.25 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onWeaponHit(ctx)\n"
		"  if ctx.weaponId == 'test.weapon' and ctx.targetType == 9 then\n"
		"    return { handled = true, applyDamage = true, damage = ctx.damage * 2, destroyTarget = true, scoreDelta = 50 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onTriggerEnter(ctx)\n"
		"  if ctx.triggerId == 'test.trigger' and ctx.triggerType == 11 then\n"
		"    return { handled = true, solid = true, deleteSelf = true, deleteOther = false, damagePlayer = 0.5, healthDelta = -0.25, scoreDelta = 75 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onObjectCollision(ctx)\n"
		"  if ctx.collisionId == 'test.collision' and ctx.collisionType == 17 then\n"
		"    assert(ctx.selfType == 21)\n"
		"    assert(ctx.otherType == 22)\n"
		"    assert(ctx.otherFlags == 23)\n"
		"    assert(ctx.sideBits == 24)\n"
		"    return { handled = true, suppressNative = true, deleteSelf = false, deleteOther = true, applyDamage = true, damage = ctx.damage + 0.75, scoreDelta = 125, healthDelta = -0.5 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onPlayerDamage(ctx)\n"
		"  if ctx.damageId == 'test.damage' and ctx.damageType == 31 then\n"
		"    assert(ctx.playerNum == 0)\n"
		"    assert(math.abs(ctx.damage - 0.4) < 0.001)\n"
		"    assert(ctx.position.x == 11)\n"
		"    assert(ctx.position.y == 12)\n"
		"    assert(ctx.position.z == 13)\n"
		"    return { handled = true, applyDamage = true, damage = ctx.damage * 0.5, healthDelta = -0.1, scoreDelta = 33 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onObjectDamage(ctx)\n"
		"  if ctx.damageId == 'test.object.damage' and ctx.damageType == 41 then\n"
		"    assert(ctx.playerNum == 0)\n"
		"    assert(math.abs(ctx.damage - 1.5) < 0.001)\n"
		"    assert(ctx.targetType == 19)\n"
		"    assert(ctx.targetFlags == 20)\n"
		"    assert(ctx.position.x == 21)\n"
		"    assert(ctx.position.y == 22)\n"
		"    assert(ctx.position.z == 23)\n"
		"    return { handled = true, applyDamage = true, damage = ctx.damage + 2.0, destroyTarget = true, scoreDelta = 44 }\n"
		"  end\n"
		"  return { handled = false }\n"
		"end\n"
		"\n"
		"function module.onObjectDelete(ctx)\n"
		"  assert(ctx.levelNum == 1)\n"
		"  assert(pangea.object.exists(ctx.object))\n"
		"  assert(ctx.position.x == 0)\n"
		"  assert(ctx.position.y == 0)\n"
		"  assert(ctx.position.z == 0)\n"
		"  assert(#ctx.tags == 2)\n"
		"  assert(ctx.tags[1] == 'pickup')\n"
		"  assert(ctx.tags[2] == 'test')\n"
		"  assert(pangea.player.setInfo(0, { score = 321 }))\n"
		"end\n"
		"\n"
		"function module.onObjectFrame(ctx)\n"
		"  local pos = pangea.object.position(ctx.object)\n"
		"  if pos then\n"
		"    local objectState = pangea.object.state(ctx.object)\n"
		"    objectState.frames = (objectState.frames or 0) + 1\n"
		"    pangea.object.setPosition(ctx.object, { x = pos.x + 1, y = pos.y + 1, z = pos.z + 1 })\n"
		"  end\n"
		"  return { positionOffset = { x = 0.1, y = 0.2, z = 0.3 } }\n"
		"end\n"
		"\n"
		"return module\n";

	write_temp_file("Data/Scripts/dist/main.lua", testScript);

	status = PangeaScript_SetStartupScript("Data/Scripts/dist/main.lua");
	assert(status == PANGEA_SCRIPT_OK);
	assert(PangeaScript_HasRunnableModule());

	PangeaScriptLevelContext lvlCtx = { 1, "Level 1" };
	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_LOAD, &lvlCtx);
	assert(status == PANGEA_SCRIPT_OK);

	assert(gSpawnedNativeDummy.position.x == 100.0f);
	assert(gSpawnedNativeDummy.position.y == 200.0f);
	assert(gSpawnedNativeDummy.position.z == 300.0f);

	status = PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &lvlCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(gDeferredDeleteDummy.deleted == true);
	assert(gDummyPlayerInfo.score == 321);
	assert(gDummyPlayerInfo.health == 0.9f);
	assert(gDummyPlayerInfo.lives == 5);
	assert(gDummyPlayerInfo.ammo == 8);
	assert(gDummyPlayerInfo.fuel == 0.5f);
	assert(gDummyPlayerInfo.shield == 1.0f);
	assert(gDummyPlayerInfo.currency == 12);
	assert(gSoundRequestCount == 1);
	assert(gLastSoundRequest.soundId == 17);
	assert(gLastSoundRequest.hasPosition);
	assert(gLastSoundRequest.position.x == 1.0f);
	assert(gLastSoundRequest.position.y == 2.0f);
	assert(gLastSoundRequest.position.z == 3.0f);
	assert(gLastSoundRequest.hasVolume);
	assert(gLastSoundRequest.volume == 0.5f);
	assert(gLastSoundRequest.hasRate);
	assert(gLastSoundRequest.rate == 1.25f);
	assert(gEffectRequestCount == 1);
	assert(gLastEffectRequest.effectId == 23);
	assert(gLastEffectRequest.hasPosition);
	assert(gLastEffectRequest.position.x == 4.0f);
	assert(gLastEffectRequest.position.y == 5.0f);
	assert(gLastEffectRequest.position.z == 6.0f);
	assert(gLastEffectRequest.hasVelocity);
	assert(gLastEffectRequest.velocity.x == 7.0f);
	assert(gLastEffectRequest.velocity.y == 8.0f);
	assert(gLastEffectRequest.velocity.z == 9.0f);
	assert(gLastEffectRequest.hasScale);
	assert(gLastEffectRequest.scale == 1.5f);
	assert(gLastEffectRequest.hasQuantity);
	assert(gLastEffectRequest.quantity == 12);

	assert(gSpawnedNativeDummy.position.x == 105.0f);
	assert(gSpawnedNativeDummy.position.y == 205.0f);
	assert(gSpawnedNativeDummy.position.z == 305.0f);
	assert(gSpawnedNativeDummy.velocity.x == 1.0f);
	assert(gSpawnedNativeDummy.velocity.y == 2.0f);
	assert(gSpawnedNativeDummy.velocity.z == 3.0f);
	assert(gSpawnedNativeDummy.health == 0.75f);
	assert(gSpawnedNativeDummy.damage == 0.25f);
	assert(gSpawnedNativeDummy.cType == 13);
	assert(gSpawnedNativeDummy.cBits == 14);
	assert(gSpawnedNativeDummy.bounds.left == -4.0f);
	assert(gSpawnedNativeDummy.bounds.right == 4.0f);
	assert(gSpawnedNativeDummy.bounds.front == -5.0f);
	assert(gSpawnedNativeDummy.bounds.back == 5.0f);
	assert(gSpawnedNativeDummy.bounds.top == 6.0f);
	assert(gSpawnedNativeDummy.bounds.bottom == -6.0f);
	assert(gSpawnedNativeDummy.params.count == 4);
	assert(gSpawnedNativeDummy.params.values[0] == 11);
	assert(gSpawnedNativeDummy.params.values[1] == 22);
	assert(gSpawnedNativeDummy.params.values[2] == 33);
	assert(gSpawnedNativeDummy.params.values[3] == 44);

	unsigned char dummyParams[2] = { 10, 20 };
	PangeaScriptTerrainItemContext terrainCtx = {
		.levelNum = 1,
		.itemType = 42,
		.remappedItemType = 42,
		.playerNum = 0,
		.networked = false,
		.x = 12.0f,
		.z = 34.0f,
		.flags = 0,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallTerrainItemHook(&terrainCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(terrainCtx.handled == true);
	assert(terrainCtx.markInUse == true);

	terrainCtx.itemType = 43;
	terrainCtx.handled = false;
	terrainCtx.markInUse = false;
	status = PangeaScript_CallTerrainItemHook(&terrainCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(terrainCtx.handled == false);

	PangeaScriptSplineItemContext splineCtx = {
		.levelNum = 1,
		.itemType = 100,
		.splineNum = 2,
		.placement = 0.5f,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallSplineItemHook(&splineCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(splineCtx.handled == true);
	assert(splineCtx.markInUse == true);

	PangeaScriptMapItemContext mapCtx = {
		.levelNum = 1,
		.sceneNum = 5,
		.areaNum = 1,
		.itemType = 50,
		.x = 15.0f,
		.y = 25.0f,
		.params = dummyParams,
		.paramCount = 2,
		.handled = false,
		.markInUse = false
	};
	status = PangeaScript_CallMapItemHook(&mapCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(mapCtx.handled == true);
	assert(mapCtx.markInUse == true);

	PangeaScriptPickupContext pickupCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.pickupId = "test.pickup",
		.pickupType = 7,
		.amount = 1,
		.pickup = { 1, 1 },
		.player = { 0, 0 },
		.position = { 15.0f, 25.0f, 35.0f },
		.handled = false,
		.consumePickup = false,
		.scoreDelta = 0,
		.healthDelta = 0.0f
	};
	status = PangeaScript_CallPickupCollectedHook(&pickupCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(pickupCtx.handled == true);
	assert(pickupCtx.consumePickup == true);
	assert(pickupCtx.scoreDelta == 250);
	assert(pickupCtx.healthDelta == 0.25f);

	PangeaScriptWeaponHitContext weaponHitCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.weaponId = "test.weapon",
		.weaponType = 3,
		.damage = 0.5f,
		.weapon = { 1, 1 },
		.target = { 2, 1 },
		.position = { 10.0f, 20.0f, 30.0f },
		.targetType = 9,
		.targetFlags = 0,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0
	};
	status = PangeaScript_CallWeaponHitHook(&weaponHitCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(weaponHitCtx.handled == true);
	assert(weaponHitCtx.applyDamage == true);
	assert(weaponHitCtx.damage == 1.0f);
	assert(weaponHitCtx.destroyTarget == true);
	assert(weaponHitCtx.scoreDelta == 50);

	PangeaScriptTriggerContext triggerCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.triggerId = "test.trigger",
		.triggerType = 11,
		.self = { 1, 1 },
		.other = { 2, 1 },
		.position = { 4.0f, 5.0f, 6.0f },
		.sideBits = 3,
		.otherType = 4,
		.otherFlags = 5,
		.handled = false,
		.solid = false,
		.deleteSelf = false,
		.deleteOther = false,
		.damagePlayer = 0.0f,
		.healthDelta = 0.0f,
		.scoreDelta = 0
	};
	status = PangeaScript_CallTriggerEnterHook(&triggerCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(triggerCtx.handled == true);
	assert(triggerCtx.solid == true);
	assert(triggerCtx.deleteSelf == true);
	assert(triggerCtx.deleteOther == false);
	assert(triggerCtx.damagePlayer == 0.5f);
	assert(triggerCtx.healthDelta == -0.25f);
	assert(triggerCtx.scoreDelta == 75);

	PangeaScriptObjectCollisionContext collisionCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.collisionId = "test.collision",
		.collisionType = 17,
		.self = { 1, 1 },
		.other = { 2, 1 },
		.position = { 7.0f, 8.0f, 9.0f },
		.sideBits = 24,
		.selfType = 21,
		.selfFlags = 20,
		.otherType = 22,
		.otherFlags = 23,
		.damage = 0.25f,
		.handled = false,
		.suppressNative = false,
		.deleteSelf = false,
		.deleteOther = false,
		.applyDamage = false,
		.scoreDelta = 0,
		.healthDelta = 0.0f
	};
	status = PangeaScript_CallObjectCollisionHook(&collisionCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(collisionCtx.handled == true);
	assert(collisionCtx.suppressNative == true);
	assert(collisionCtx.deleteSelf == false);
	assert(collisionCtx.deleteOther == true);
	assert(collisionCtx.applyDamage == true);
	assert(collisionCtx.damage == 1.0f);
	assert(collisionCtx.scoreDelta == 125);
	assert(collisionCtx.healthDelta == -0.5f);

	PangeaScriptPlayerDamageContext damageCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.damageId = "test.damage",
		.damageType = 31,
		.damage = 0.4f,
		.source = { 2, 1 },
		.player = { 1, 1 },
		.position = { 11.0f, 12.0f, 13.0f },
		.handled = false,
		.applyDamage = true,
		.healthDelta = 0.0f,
		.scoreDelta = 0
	};
	status = PangeaScript_CallPlayerDamageHook(&damageCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(damageCtx.handled == true);
	assert(damageCtx.applyDamage == true);
	assert(damageCtx.damage == 0.2f);
	assert(damageCtx.healthDelta == -0.1f);
	assert(damageCtx.scoreDelta == 33);

	PangeaScriptObjectDamageContext objectDamageCtx = {
		.levelNum = 1,
		.playerNum = 0,
		.damageId = "test.object.damage",
		.damageType = 41,
		.damage = 1.5f,
		.source = { 2, 1 },
		.target = { 1, 1 },
		.position = { 21.0f, 22.0f, 23.0f },
		.targetType = 19,
		.targetFlags = 20,
		.handled = false,
		.applyDamage = false,
		.destroyTarget = false,
		.scoreDelta = 0
	};
	status = PangeaScript_CallObjectDamageHook(&objectDamageCtx);
	assert(status == PANGEA_SCRIPT_OK);
	assert(objectDamageCtx.handled == true);
	assert(objectDamageCtx.applyDamage == true);
	assert(objectDamageCtx.damage == 3.5f);
	assert(objectDamageCtx.destroyTarget == true);
	assert(objectDamageCtx.scoreDelta == 44);

	PangeaScriptObjectHandle nativeHandle = { 1, 1 };
	PangeaScriptFrameContext frameCtx = {
		.levelNum = 1,
		.frameNum = 1,
		.deltaSeconds = 0.016f,
		.levelTimeSeconds = 0.016f
	};
	PangeaScriptObjectFrameResult frameResult;
	status = PangeaScript_CallObjectFrame(nativeHandle, &frameCtx, &frameResult);
	assert(status == PANGEA_SCRIPT_OK);
	assert(frameResult.hasPositionOffset == true);
	assert(frameResult.positionOffset.x == 0.1f);
	assert(frameResult.positionOffset.y == 0.2f);
	assert(frameResult.positionOffset.z == 0.3f);

	assert(gSpawnedNativeDummy.position.x == 106.0f);
	assert(gSpawnedNativeDummy.position.y == 206.0f);
	assert(gSpawnedNativeDummy.position.z == 306.0f);

	system("rm -rf Data");

	PangeaScript_Shutdown();
	printf("Lua backend integration tests passed successfully!\n");
}

int main(void)
{
	printf("=============================================\n");
	printf(" Running Lua Backend Integration Tests       \n");
	printf("=============================================\n");

	test_lua_integration();

	printf("=============================================\n");
	printf(" All Lua Backend Integration Tests Passed    \n");
	printf("=============================================\n");
	return 0;
}
