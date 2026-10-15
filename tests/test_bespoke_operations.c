#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int gActor, gCalls;
static PangeaScriptObjectHandle gItem, gCarried;
static PangeaScriptOperationRequest gLastRequest;
static bool gStaleOutput, gBadRelations, gReenter;

static int player_count(void) { return 1; }

static bool get_position(void* object, PangeaScriptVector3* output)
{
	assert(object == &gActor);
	*output = (PangeaScriptVector3){0};
	return true;
}

static bool get_player(int player, PangeaScriptPlayerSnapshot* output)
{
	if (player != 0) return false;
	*output = (PangeaScriptPlayerSnapshot){0};
	return true;
}

static PangeaScriptStatus operate(const PangeaScriptOperationRequest* request, PangeaScriptOperationResult* output)
{
	gCalls++;
	gLastRequest = *request;
	if (gReenter)
	{
		PangeaScriptOperationRequest nested = {.operation = PANGEA_SCRIPT_OP_FIRE};
		PangeaScriptOperationResult result;
		gReenter = false;
		assert(PangeaScript_RunPlayerOperation(&nested, &result) == PANGEA_SCRIPT_RUNTIME_ERROR);
		assert(!result.object.id);
	}
	if (request->playerNum != 0) return PANGEA_SCRIPT_BAD_ARGUMENT;
	if (request->mode && strcmp(request->mode, "reject") == 0) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
	if (request->mode && strcmp(request->mode, "unknown") == 0) { output->object = gItem; return PANGEA_SCRIPT_CONFIG_ERROR + 1; }
	if (request->operation == PANGEA_SCRIPT_OP_CARRY)
	{
		if (gCarried.id) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		gCarried = request->target;
		output->object = gCarried;
	}
	else if (request->operation == PANGEA_SCRIPT_OP_DROP)
	{
		if (!gCarried.id) return PANGEA_SCRIPT_INCOMPATIBLE_ITEM;
		output->object = gCarried;
		gCarried = (PangeaScriptObjectHandle){0};
	}
	else if (request->operation == PANGEA_SCRIPT_OP_FIRE)
	{
		output->object = gItem;
	}
	if (gStaleOutput) output->object = (PangeaScriptObjectHandle){.id = 9999, .generation = 1};
	return PANGEA_SCRIPT_OK;
}

static bool relations(int player, PangeaScriptPlayerRelations* output)
{
	if (player != 0) return false;
	*output = (PangeaScriptPlayerRelations){.carried = gCarried, .form = "normal", .capturedCount = 2};
	if (gBadRelations) output->carried = (PangeaScriptObjectHandle){.id = 9999, .generation = 1};
	return true;
}

static void register_item(void)
{
	static const PangeaScriptObjectOps ops = {.getPosition = get_position};
	const PangeaScriptObjectRegistration registration = {.nativeObject = &gActor, .objectType = "carryable",
		.ops = &ops, .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL};
	assert(PangeaScript_RegisterObject(&registration, &gItem) == PANGEA_SCRIPT_OK);
}

static void assert_empty_relations(const PangeaScriptPlayerRelations* state)
{
	assert(!state->carried.id && !state->carried.generation && !state->mounted.id && !state->mounted.generation);
	assert(!state->buddy.id && !state->buddy.generation && !state->grabbed.id && !state->grabbed.generation);
	assert(!state->form && !state->capturedCount);
}

static void test_relation_transitions(void)
{
	PangeaScriptOperationResult result;
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_CARRY, .hasTarget = true, .target = gItem};
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(result.object.id == gItem.id && result.object.generation == gItem.generation);
	PangeaScriptPlayerRelations state;
	assert(PangeaScript_GetPlayerRelations(0, &state));
	assert(state.carried.id == gItem.id && strcmp(state.form, "normal") == 0 && state.capturedCount == 2);
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	assert(PangeaScript_GetPlayerRelations(0, &state) && state.carried.id == gItem.id);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_DROP, .hasVelocity = true, .velocity = {1, 2, 3}};
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(gLastRequest.velocity.z == 3 && result.object.id == gItem.id);
	assert(PangeaScript_GetPlayerRelations(0, &state) && !state.carried.id);
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
}

static void test_invalid_boundaries(void)
{
	PangeaScriptOperationResult result = {.object = gItem};
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_FIRE};
	int before = gCalls;
	request.playerNum = -1;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.playerNum = 0; request.operation = PANGEA_SCRIPT_OP_COUNT;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.operation = PANGEA_SCRIPT_OP_FIRE; request.hasAim = true; request.aim.x = NAN;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasAim = false; request.hasPosition = true; request.position.y = INFINITY;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasPosition = false; request.hasVelocity = true; request.velocity.z = NAN;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasVelocity = false; request.mode = "";
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.mode = NULL;
	request.hasVelocity = false; request.hasTarget = true; request.target = (PangeaScriptObjectHandle){.id = 9999, .generation = 1};
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasTarget = false; request.hasWeapon = true; request.weapon = -1;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	request.hasWeapon = false; request.hasJoint = true; request.joint = -2;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gCalls == before);
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_MOUNT, .hasTarget = true, .target = gItem};
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	assert(!PangeaScript_OperationSupported(PANGEA_SCRIPT_OP_MOUNT));
	assert(PangeaScript_OperationSupported(PANGEA_SCRIPT_OP_FIRE));
	assert(!PangeaScript_OperationSupported(PANGEA_SCRIPT_OP_COUNT));
	request.operation = PANGEA_SCRIPT_OP_FIRE; request.mode = "reject";
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	assert(!result.object.id);
	request.mode = "unknown";
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_RUNTIME_ERROR && !result.object.id);
	PangeaScriptPlayerRelations state = {.carried = gItem, .mounted = gItem, .buddy = gItem, .grabbed = gItem, .form = "previous", .capturedCount = 3};
	assert(!PangeaScript_GetPlayerRelations(-1, &state)); assert_empty_relations(&state);
	state.carried = gItem;
	assert(!PangeaScript_GetPlayerRelations(1, &state)); assert_empty_relations(&state);
	assert(!PangeaScript_GetPlayerRelations(0, NULL));
}

static void test_network_and_stale(void)
{
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_FIRE};
	PangeaScriptOperationResult result;
	int before = gCalls;
	PangeaScript_SetNetworkedMode(true);
	assert(PangeaScript_RunPlayerOperation(&request, &result) != PANGEA_SCRIPT_OK);
	assert(gCalls == before);
	PangeaScript_SetNetworkedMode(false);
	gStaleOutput = true;
	assert(PangeaScript_RunPlayerOperation(&request, &result) != PANGEA_SCRIPT_OK);
	assert(!result.object.id);
	gStaleOutput = false;
	before = gCalls;
	gReenter = true;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_OK);
	assert(gCalls == before + 1);
	gBadRelations = true;
	PangeaScriptPlayerRelations state = {.carried = gItem, .mounted = gItem, .buddy = gItem, .grabbed = gItem, .form = "previous", .capturedCount = 3};
	assert(!PangeaScript_GetPlayerRelations(0, &state));
	assert_empty_relations(&state);
	gBadRelations = false;
	request = (PangeaScriptOperationRequest){.operation = PANGEA_SCRIPT_OP_CARRY, .hasTarget = true, .target = gItem};
	PangeaScript_UnregisterObject(gItem);
	before = gCalls;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_BAD_ARGUMENT);
	assert(gCalls == before);
	register_item();
}

static void test_absent_operations(void)
{
	PangeaScript_Shutdown();
	PangeaScriptGameplayOps missing = {.operationMask = 1u << PANGEA_SCRIPT_OP_FIRE};
	PangeaScriptGameInfo game = {.gameId = "OttoMatic-Android", .gameName = "Missing native operation", .getPlayerCount = player_count, .gameplay = &missing};
	assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
	assert(!PangeaScript_OperationSupported(PANGEA_SCRIPT_OP_FIRE));
	PangeaScriptOperationRequest request = {.operation = PANGEA_SCRIPT_OP_FIRE};
	PangeaScriptOperationResult result;
	assert(PangeaScript_RunPlayerOperation(&request, &result) == PANGEA_SCRIPT_INCOMPATIBLE_ITEM);
	PangeaScriptPlayerRelations state;
	assert(!PangeaScript_GetPlayerRelations(0, &state));
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&game);
	assert(backend);
	char error[1024] = {0};
	assert(PangeaScriptBackend_Load(backend, "return {onFrame=function() assert(#pangea.player.actions()==0) end}", error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {0};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
	PangeaScript_Shutdown();
}

static void call_frame(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	PangeaScriptFrameContext frame = {.deltaSeconds = .1f};
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error));
	if (status != PANGEA_SCRIPT_OK) fprintf(stderr, "Bespoke Lua test failed: %s\n", error);
	assert(status == PANGEA_SCRIPT_OK);
}

static void test_lua(const PangeaScriptGameInfo* game)
{
	char source[8192], error[1024] = {0};
	snprintf(source, sizeof(source),
		"local item={id=%d,generation=%u} return {onFrame=function() "
		"local actions=pangea.player.actions() local found={} "
		"for _,action in ipairs(actions) do found[action]=true end "
		"assert(#actions==4 and found.carry and found.drop and found.fire and found.transform and not found.mount) "
		"assert(pangea.player.relations(0).form=='normal') "
		"assert(pangea.player.carry(0,item).ok) "
		"assert(pangea.player.relations(0).carried.id==item.id) "
		"assert(not pangea.player.carry(0,item).ok) "
		"assert(pangea.player.drop(0,{velocity={x=1,y=2,z=3}}).ok) "
		"assert(pangea.player.fire(0,{weapon=2,aim={x=0,y=1,z=0},mode='primary'}).ok) "
		"assert(pangea.player.transform(0,'normal').ok) "
		"assert(not pangea.player.mount(0,item).ok) "
		"assert(not pangea.player.fire(0,{mode='reject'}).ok) "
		"end}", gItem.id, gItem.generation);
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(game);
	assert(backend);
	assert(PangeaScriptBackend_Load(backend, source, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	call_frame(backend);
	assert(gLastRequest.operation == PANGEA_SCRIPT_OP_FIRE && strcmp(gLastRequest.mode, "reject") == 0);
	const char* invalid =
		"return {onFrame=function() "
		"local callbacks=0 local mt={__index=function() callbacks=callbacks+1 return 0 end} "
		"local function rejected(result) assert(not result.ok and result.reason=='bad-argument' and result.object==nil) end "
		"assert(not pangea.player.fire('0').ok) "
		"assert(not pangea.player.fire(0.5).ok) "
		"rejected(pangea.player.fire(-1)) rejected(pangea.player.fire(1)) "
		"rejected(pangea.player.fire(2147483648)) "
		"assert(not pangea.player.fire(0,{weapon='1'}).ok) "
		"assert(not pangea.player.fire(0,{weapon=1.5}).ok) "
		"rejected(pangea.player.fire(0,{weapon=2147483648})) "
		"rejected(pangea.player.fire(0,{joint=0.5})) "
		"assert(not pangea.player.fire(0,{aim={x=0/0,y=0,z=0}}).ok) "
		"rejected(pangea.player.fire(0,{['aim\\0extra']={x=0,y=1,z=0}})) "
		"rejected(pangea.player.fire(0,{aim=setmetatable({x=0,y=1,z=0},mt)})) "
		"rejected(pangea.player.fire(0,{target=setmetatable({id=1,generation=1},mt)})) "
		"rejected(pangea.player.carry(0,setmetatable({id=1,generation=1},mt))) "
		"rejected(pangea.player.mount(0,{id=1.5,generation=1})) "
		"rejected(pangea.player.mount(0,{id=2147483648,generation=1})) "
		"rejected(pangea.player.mount(0,{id=1,generation=1.5})) "
		"assert(not pangea.player.fire(0,{position={x=1,y=2}}).ok) "
		"assert(not pangea.player.fire(0,{mode=true}).ok) "
		"assert(not pangea.player.fire(0,{unknown=true}).ok) "
		"assert(not pangea.player.carry(0,{id=9999,generation=1}).ok) "
		"assert(not pangea.player.transform(0,1).ok) "
		"rejected(pangea.player.transform(0,'normal\\0extra')) "
		"assert(not pangea.player.fire(0,setmetatable({},{__index=function() error('no metamethod') end})).ok) "
		"assert(callbacks==0) "
		"end}";
	assert(PangeaScriptBackend_Load(backend, invalid, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	int before = gCalls;
	call_frame(backend);
	assert(gCalls == before);
	const char* stale = "return {onFrame=function() local result=pangea.player.fire(0) "
		"assert(not result.ok and result.reason=='runtime-error' and result.object==nil) "
		"assert(pangea.player.relations(0)==nil) end}";
	assert(PangeaScriptBackend_Load(backend, stale, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	gStaleOutput = true; gBadRelations = true;
	call_frame(backend);
	gStaleOutput = false; gBadRelations = false;
	const char* rejected = "return {onFrame=function() local result=pangea.player.fire(0,{mode='reject'}) "
		"assert(not result.ok and result.reason=='incompatible-item' and result.object==nil) end}";
	assert(PangeaScriptBackend_Load(backend, rejected, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	call_frame(backend);
	before = gCalls;
	const char* original = "return {onFrame=function() assert(pangea.player.fire(0).ok) end}";
	assert(PangeaScriptBackend_Load(backend, original, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_Reload(backend, "pangea.player.fire(0) return {}", error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(gCalls == before);
	call_frame(backend);
	assert(gCalls == before + 1);
	PangeaScriptBackend_Destroy(backend);
}

int main(void)
{
	const PangeaScriptGameplayOps gameplay = {.operationMask = (1u << PANGEA_SCRIPT_OP_CARRY) | (1u << PANGEA_SCRIPT_OP_DROP)
		| (1u << PANGEA_SCRIPT_OP_FIRE) | (1u << PANGEA_SCRIPT_OP_TRANSFORM), .runOperation = operate, .getRelations = relations};
	const PangeaScriptGameInfo game = {.gameId = "OttoMatic-Android", .gameName = "Bespoke operations",
		.getPlayer = get_player, .getPlayerCount = player_count, .gameplay = &gameplay};
	assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
	register_item();
	test_relation_transitions();
	test_invalid_boundaries();
	test_network_and_stale();
	test_lua(&game);
	test_absent_operations();
	puts("Bespoke operation tests passed");
	return 0;
}
