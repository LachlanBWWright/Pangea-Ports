#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct Actor { PangeaScriptVector3 position; float health, damage; } Actor;
static Actor gParent, gChild;
static PangeaScriptObjectHandle gParentHandle, gChildHandle;
static bool gBadInput, gBadTerrain, gBadOverlap;
static int gMutations;
static double gFuel = 5;

static bool get_position(void* actor, PangeaScriptVector3* position) { *position = ((Actor*)actor)->position; return true; }
static bool set_position(void* actor, const PangeaScriptVector3* position) { ((Actor*)actor)->position = *position; return true; }
static bool set_health(void* actor, float value) { ((Actor*)actor)->health = value; gMutations++; return true; }
static bool set_damage(void* actor, float value) { ((Actor*)actor)->damage = value; gMutations++; return true; }
static bool joint_position(void* actor, int joint, PangeaScriptVector3* position)
{
	if (joint != 0) return false;
	*position = ((Actor*)actor)->position; position->y += 2;
	return true;
}
static bool input(int player, PangeaScriptInputSnapshot* output)
{
	if (player != 0) return false;
	*output = (PangeaScriptInputSnapshot){.actionCount = 1, .moveX = .5f};
	output->actions[0] = (PangeaScriptInputAction){.name = "fire", .held = true, .pressed = true};
	if (gBadInput) output->actionCount = PANGEA_SCRIPT_INPUT_ACTIONS + 1;
	return true;
}
static bool terrain(float x, float z, PangeaScriptTerrainSample* output)
{
	*output = (PangeaScriptTerrainSample){.height = x+z, .normal = {0,1,0}, .surfaceType = 2, .hasWater = true, .waterHeight = 4};
	if (gBadTerrain) output->height = NAN;
	return true;
}
static bool tile(float x, float y, PangeaScriptTileSample* output)
{
	(void)x; (void)y;
	*output = (PangeaScriptTileSample){.tileType = 7, .flags = 3, .solid = true};
	return true;
}
static bool overlap(const PangeaScriptBounds* bounds, unsigned int mask, PangeaScriptObjectHandle* results, int capacity, int* count)
{
	(void)bounds;
	assert(mask == 1 && capacity > 0);
	results[0] = gChildHandle;
	*count = gBadOverlap ? capacity+1 : 1;
	return true;
}
static bool get_state(int player, const char* key, PangeaScriptStateValue* output)
{
	if (player != 0 || strcmp(key,"fuel")) return false;
	*output = (PangeaScriptStateValue){.type = PANGEA_SCRIPT_STATE_NUMBER, .number = gFuel};
	return true;
}
static PangeaScriptStatus set_state(int player, const char* key, const PangeaScriptStateValue* value)
{
	if (player != 0 || strcmp(key,"fuel")) return PANGEA_SCRIPT_BAD_ARGUMENT;
	gFuel = value->number; gMutations++;
	return PANGEA_SCRIPT_OK;
}
static PangeaScriptStatus complete(int player, bool success)
{
	if (player != 0 || !success) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	gMutations++;
	return PANGEA_SCRIPT_OK;
}

static void test_boundaries(void)
{
	assert(PangeaScript_SetObjectScriptControlled(gChildHandle,true));
	assert(PangeaScript_ObjectIsScriptControlled(gChildHandle));
	assert(PangeaScript_SetObjectHealth(gChildHandle,2));
	assert(PangeaScript_SetObjectDamage(gChildHandle,3));
	assert(gChild.health == 2 && gChild.damage == 3);
	assert(!PangeaScript_SetObjectHealth(gChildHandle,NAN));
	assert(!PangeaScript_SetObjectDamage(gChildHandle,-1));
	PangeaScriptInputSnapshot actions;
	assert(PangeaScript_GetInput(0,&actions) && actions.actions[0].pressed);
	assert(!PangeaScript_GetInput(-1,&actions));
	gBadInput = true; assert(!PangeaScript_GetInput(0,&actions)); gBadInput = false;
	PangeaScriptTerrainSample sample;
	assert(PangeaScript_SampleTerrain(2,3,&sample) && sample.height == 5 && sample.waterHeight == 4);
	assert(!PangeaScript_SampleTerrain(NAN,3,&sample));
	gBadTerrain = true; assert(!PangeaScript_SampleTerrain(2,3,&sample)); gBadTerrain = false;
	PangeaScriptBounds bounds = {.min = {-1,-1,-1}, .max = {1,1,1}};
	PangeaScriptObjectHandle hits[2]; int count;
	assert(PangeaScript_OverlapBox(&bounds,"actors",hits,2,&count) && count == 1);
	assert(!PangeaScript_OverlapBox(&bounds,"unknown",hits,2,&count));
	gBadOverlap = true; assert(!PangeaScript_OverlapBox(&bounds,"actors",hits,2,&count)); gBadOverlap = false;
	bounds.min.x = 2; assert(!PangeaScript_OverlapBox(&bounds,"actors",hits,2,&count));
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 8};
	assert(PangeaScript_SetState(0,"fuel",&value) == PANGEA_SCRIPT_OK && gFuel == 8);
	value.number = 11; assert(PangeaScript_SetState(0,"fuel",&value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	value.type = PANGEA_SCRIPT_STATE_BOOLEAN; assert(PangeaScript_SetState(0,"fuel",&value) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(PangeaScript_SetState(0,"readonly",&value) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	assert(PangeaScript_Complete(0,false) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
}

static void test_attachments(void)
{
	PangeaScriptVector3 offset = {1,2,3};
	assert(PangeaScript_SetObjectScriptControlled(gChildHandle,false));
	assert(PangeaScript_AttachObject(gChildHandle,gParentHandle,0,&offset));
	assert(!PangeaScript_AttachObject(gParentHandle,gChildHandle,-1,&offset));
	assert(!PangeaScript_SetObjectScriptControlled(gChildHandle,false));
	PangeaScript_UpdateAttachments();
	assert(gChild.position.x == 11 && gChild.position.y == 24 && gChild.position.z == 33);
	assert(PangeaScript_DetachObject(gChildHandle));
	assert(!PangeaScript_ObjectIsScriptControlled(gChildHandle));
	assert(PangeaScript_AttachObject(gParentHandle,gChildHandle,-1,&offset));
	assert(PangeaScript_DetachObject(gParentHandle));
	assert(!PangeaScript_AttachObject(gChildHandle,gParentHandle,99,&offset));
}

static void test_lua(const PangeaScriptGameInfo* game)
{
	char error[1024] = {0}, source[4096];
	snprintf(source,sizeof(source),
		"local child={id=%d,generation=%u} local parent={id=%d,generation=%u} "
		"return {onFrame=function() "
		"assert(not pangea.level.rulesControlled()) "
		"assert(not pangea.level.setRulesControlled(1).ok) "
		"assert(not pangea.level.setRulesControlled('true').ok) "
		"assert(pangea.level.setRulesControlled(true).ok) "
		"assert(pangea.level.rulesControlled()) "
		"assert(pangea.level.setRulesControlled(false).ok) "
		"assert(not pangea.level.rulesControlled()) "
		"assert(pangea.input.get(0).actions.fire.pressed) "
		"assert(pangea.world.sampleTerrain(2,3).height==5) "
		"assert(pangea.world.sampleTile(1,2).solid) "
		"assert(#pangea.world.overlapBox({min={x=-1,y=-1,z=-1},max={x=1,y=1,z=1}},'actors')==1) "
		"assert(pangea.object.setScriptControlledResult(child,true).ok) "
		"assert(pangea.object.setHealthResult(child,4).ok) "
		"assert(pangea.object.setDamageResult(child,2).ok) "
		"assert(pangea.object.scriptControlled(child)) "
		"assert(pangea.player.getState(0,'fuel')==8) "
		"assert(pangea.player.setState(0,'fuel',7).ok) "
		"assert(not pangea.player.setState(0,'fuel',11).ok) "
		"assert(pangea.object.attach(child,parent,{joint=-1,offset={x=1,y=0,z=0}}).ok) "
		"assert(pangea.object.detach(child).ok) "
		"assert(pangea.level.complete(0,true).ok) end}",
		gChildHandle.id,gChildHandle.generation,gParentHandle.id,gParentHandle.generation);
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(game);
	assert(backend);
	PangeaScriptStatus status = PangeaScriptBackend_Load(backend,source,error,sizeof(error));
	assert(status == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.deltaSeconds = .1f};
	status = PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error));
	if (status != PANGEA_SCRIPT_OK) fprintf(stderr,"Lua gameplay test failed: %s\n",error);
	assert(status == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
}

static void test_network_and_stale(void)
{
	int mutations = gMutations;
	assert(PangeaScript_SetRulesScriptControlled(true) == PANGEA_SCRIPT_OK);
	PangeaScript_SetNetworkedMode(true);
	assert(PangeaScript_SetRulesScriptControlled(false) != PANGEA_SCRIPT_OK);
	assert(PangeaScript_RulesAreScriptControlled());
	PangeaScriptStateValue value = {.type = PANGEA_SCRIPT_STATE_NUMBER, .number = 6};
	assert(!PangeaScript_SetObjectHealth(gChildHandle,1));
	assert(!PangeaScript_SetObjectScriptControlled(gChildHandle,false));
	assert(PangeaScript_SetState(0,"fuel",&value) != PANGEA_SCRIPT_OK);
	assert(PangeaScript_Complete(0,true) != PANGEA_SCRIPT_OK);
	assert(gMutations == mutations);
	PangeaScript_SetNetworkedMode(false);
	assert(PangeaScript_SetRulesScriptControlled(false) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_RulesAreScriptControlled());
	PangeaScriptVector3 offset = {0};
	assert(PangeaScript_AttachObject(gChildHandle,gParentHandle,-1,&offset));
	PangeaScript_UnregisterObject(gParentHandle);
	PangeaScript_UpdateAttachments();
	assert(!PangeaScript_AttachObject(gChildHandle,gParentHandle,-1,&offset));
	assert(!PangeaScript_SetObjectHealth(gParentHandle,1));
}

int main(void)
{
	const PangeaScriptStateField fields[] = {{"fuel",PANGEA_SCRIPT_STATE_NUMBER,true,0,10,true},{"readonly",PANGEA_SCRIPT_STATE_BOOLEAN,false,0,0,true}};
	const PangeaScriptCollisionCategory categories[] = {{"actors",1}};
	const PangeaScriptGameplayOps gameplay = {.getInput = input, .sampleTerrain = terrain, .sampleTile = tile, .overlapBox = overlap,
		.collisionCategories = categories, .collisionCategoryCount = 1, .getState = get_state, .setState = set_state,
		.stateFields = fields, .stateFieldCount = 2, .complete = complete};
	const PangeaScriptGameInfo game = {.gameId = "OttoMatic-Android", .gameName = "Gameplay services", .gameplay = &gameplay};
	assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
	const PangeaScriptObjectOps ops = {.getPosition = get_position, .setPosition = set_position, .setHealth = set_health, .setDamage = set_damage, .getJointPosition = joint_position};
	gParent.position = (PangeaScriptVector3){10,20,30};
	PangeaScriptObjectRegistration registration = {.nativeObject = &gParent, .ops = &ops, .objectType = "parent", .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL};
	assert(PangeaScript_RegisterObject(&registration,&gParentHandle) == PANGEA_SCRIPT_OK);
	registration.nativeObject = &gChild; registration.objectType = "child";
	assert(PangeaScript_RegisterObject(&registration,&gChildHandle) == PANGEA_SCRIPT_OK);
	test_boundaries(); test_attachments(); test_lua(&game); test_network_and_stale();
	assert(PangeaScript_SetRulesScriptControlled(true) == PANGEA_SCRIPT_OK);
	assert(PangeaScript_RulesAreScriptControlled());
	PangeaScript_Shutdown();
	assert(!PangeaScript_RulesAreScriptControlled());
	puts("Gameplay service tests passed");
	return 0;
}
