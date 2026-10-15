#include "../shared/script/pangea_script_backend.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct Collider { PangeaScriptBounds bounds; bool available; } Collider;
static Collider gNear, gFar, gMover;
static PangeaScriptObjectHandle gNearHandle, gFarHandle, gMoverHandle;
static bool gOverflow;
static PangeaScriptBounds gQueriedBounds;

static bool get_position(void* collider, PangeaScriptVector3* output)
{
	Collider* object = collider;
	*output = (PangeaScriptVector3){(object->bounds.min.x+object->bounds.max.x)*.5f,
		(object->bounds.min.y+object->bounds.max.y)*.5f,(object->bounds.min.z+object->bounds.max.z)*.5f};
	return true;
}

static bool set_position(void* collider, const PangeaScriptVector3* position)
{
	Collider* object = collider;
	PangeaScriptVector3 old;
	get_position(collider,&old);
	object->bounds.min.x += position->x-old.x; object->bounds.max.x += position->x-old.x;
	object->bounds.min.y += position->y-old.y; object->bounds.max.y += position->y-old.y;
	object->bounds.min.z += position->z-old.z; object->bounds.max.z += position->z-old.z;
	return true;
}

static bool get_bounds(void* collider, PangeaScriptBounds* output)
{
	Collider* object = collider;
	*output = object->bounds;
	return object->available;
}

static bool overlap(const PangeaScriptBounds* bounds, unsigned int mask, PangeaScriptObjectHandle* output, int capacity, int* count)
{
	assert(mask == 1 && capacity == 64);
	gQueriedBounds = *bounds;
	if (gOverflow)
	{
		for (int i = 0; i < capacity; i++) output[i] = gNearHandle;
		*count = capacity;
	}
	else
	{
		output[0] = gFarHandle; output[1] = gNearHandle;
		*count = 2;
	}
	return true;
}

static bool close_float(float actual, float expected) { return fabsf(actual-expected) < .00001f; }

static void test_rays(void)
{
	PangeaScriptVector3 from = {0,0,0}, to = {10,0,0};
	PangeaScriptCollisionHit hit;
	assert(PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && close_float(hit.fraction,.2f));
	assert(hit.position.x == 2 && hit.normal.x == -1);
	assert(gQueriedBounds.min.x == 0 && gQueriedBounds.max.x == 10);
	assert(PangeaScript_Raycast(&from,&to,"solid",gNearHandle,&hit));
	assert(hit.object.id == gFarHandle.id && close_float(hit.fraction,.6f));
	assert(PangeaScript_Raycast(&to,&from,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gFarHandle.id && close_float(hit.fraction,.3f) && hit.normal.x == 1);
	from.y = 3; to.y = 3;
	assert(PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit) && hit.object.id == 0);
	from = (PangeaScriptVector3){2.5f,0,0}; to = (PangeaScriptVector3){10,0,0};
	assert(PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && hit.fraction == 0);
	from.x = NAN;
	assert(!PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
}

static void test_sweeps(void)
{
	PangeaScriptBounds bounds = {.min = {-.5f,-.5f,-.5f}, .max = {.5f,.5f,.5f}};
	PangeaScriptVector3 delta = {10,0,0};
	PangeaScriptCollisionHit hit;
	assert(PangeaScript_SweepBox(&bounds,&delta,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && close_float(hit.fraction,.15f));
	assert(close_float(hit.position.x,1.5f) && hit.normal.x == -1);
	assert(gQueriedBounds.min.x == -.5f && gQueriedBounds.max.x == 10.5f);
	bounds.min.x = 1; bounds.max.x = 2;
	delta = (PangeaScriptVector3){-1,0,0};
	assert(PangeaScript_SweepBox(&bounds,&delta,"solid",(PangeaScriptObjectHandle){0},&hit) && hit.object.id == 0);
	delta = (PangeaScriptVector3){0,1,0};
	assert(PangeaScript_SweepBox(&bounds,&delta,"solid",(PangeaScriptObjectHandle){0},&hit) && hit.object.id == 0);
	delta = (PangeaScriptVector3){1,0,0};
	assert(PangeaScript_SweepBox(&bounds,&delta,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && hit.fraction == 0);
	bounds.min.x = 5; bounds.max.x = 4;
	assert(!PangeaScript_SweepBox(&bounds,&delta,"solid",(PangeaScriptObjectHandle){0},&hit));
}

static void test_2d_and_diagonal(void)
{
	gNear.bounds = (PangeaScriptBounds){.min = {2,2,0}, .max = {3,3,0}};
	gFar.bounds.min.z = gFar.bounds.max.z = 0;
	PangeaScriptVector3 from = {0,0,0}, to = {10,10,0};
	PangeaScriptCollisionHit hit;
	assert(PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && close_float(hit.fraction,.2f));
	PangeaScriptBounds bounds = {.min = {-.5f,-.5f,0}, .max = {.5f,.5f,0}};
	assert(PangeaScript_SweepBox(&bounds,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(hit.object.id == gNearHandle.id && close_float(hit.fraction,.15f));
	assert(close_float(hit.position.y,1.5f));
}

static void test_incomplete_queries(void)
{
	PangeaScriptVector3 from = {0}, to = {10,0,0};
	PangeaScriptCollisionHit hit;
	gOverflow = true;
	assert(!PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	gOverflow = false;
	gNear.available = false;
	assert(!PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	gNear.available = true;
	gNear.bounds.min.x = NAN;
	assert(!PangeaScript_Raycast(&from,&to,"solid",(PangeaScriptObjectHandle){0},&hit));
	assert(!PangeaScript_Raycast(&from,&to,"unknown",(PangeaScriptObjectHandle){0},&hit));
}

static void test_kinematic_movement(void)
{
	PangeaScriptVector3 delta = {10,2,0};
	PangeaScriptKinematicResult result;
	assert(PangeaScript_MoveKinematic(gMoverHandle,&delta,"solid",&result) != PANGEA_SCRIPT_OK);
	assert(PangeaScript_SetObjectScriptControlled(gMoverHandle,true));
	assert(PangeaScript_MoveKinematic(gMoverHandle,&delta,"solid",&result) == PANGEA_SCRIPT_OK);
	assert(result.position.x <= 1.5f && result.position.x > 1.49f);
	assert(fabsf(result.position.y-2) < .001f && result.contactCount >= 1);
	PangeaScriptVector3 previous = result.position;
	gOverflow = true;
	assert(PangeaScript_MoveKinematic(gMoverHandle,&delta,"solid",&result) != PANGEA_SCRIPT_OK);
	gOverflow = false;
	PangeaScriptVector3 actual;
	get_position(&gMover,&actual);
	assert(close_float(actual.x,previous.x) && close_float(actual.y,previous.y));
	assert(PangeaScript_MoveKinematic(gMoverHandle,&delta,"unknown",&result) != PANGEA_SCRIPT_OK);
	get_position(&gMover,&actual);
	assert(close_float(actual.x,previous.x) && close_float(actual.y,previous.y));
}

static void test_lua_geometry(const PangeaScriptGameInfo* game)
{
	gMover.bounds = (PangeaScriptBounds){.min = {-.5f,-.5f,-.5f}, .max = {.5f,.5f,.5f}};
	char source[2048], error[1024] = {0};
	snprintf(source,sizeof(source),
		"local mover={id=%d,generation=%u} return {onFrame=function() "
		"local hit=pangea.world.raycast({x=0,y=0,z=0},{x=10,y=0,z=0},'solid') "
		"assert(hit and math.abs(hit.fraction-.2)<.00001 and hit.normal.x==-1) "
		"assert(pangea.world.raycast({x=0,y=3,z=0},{x=10,y=3,z=0},'solid')==nil) "
		"local swept=pangea.world.sweepBox(pangea.object.bounds(mover),{x=10,y=0,z=0},'solid',mover) "
		"assert(swept and math.abs(swept.fraction-.15)<.00001) "
		"assert(pangea.object.setScriptControlled(mover,true)) "
		"local moved=pangea.object.moveKinematic(mover,{x=10,y=2,z=0},'solid') "
		"assert(moved.ok and moved.position.x<=1.5 and #moved.contacts>=1) end}",
		gMoverHandle.id,gMoverHandle.generation);
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(game);
	assert(backend);
	assert(PangeaScriptBackend_Load(backend,source,error,sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame = {.deltaSeconds = .1f};
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error));
	if (status != PANGEA_SCRIPT_OK) fprintf(stderr,"Lua geometry failed: %s\n",error);
	assert(status == PANGEA_SCRIPT_OK);
	PangeaScriptBackend_Destroy(backend);
}

int main(void)
{
	const PangeaScriptCollisionCategory categories[] = {{"solid",1}};
	const PangeaScriptGameplayOps gameplay = {.overlapBox = overlap, .collisionCategories = categories, .collisionCategoryCount = 1};
	const PangeaScriptGameInfo game = {.gameId = "OttoMatic-Android", .gameName = "Geometry", .gameplay = &gameplay};
	assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
	gNear = (Collider){.bounds = {.min = {2,-1,-1}, .max = {3,1,1}}, .available = true};
	gFar = (Collider){.bounds = {.min = {6,-1,-1}, .max = {7,1,1}}, .available = true};
	const PangeaScriptObjectOps ops = {.getPosition = get_position, .setPosition = set_position, .getBounds = get_bounds};
	PangeaScriptObjectRegistration registration = {.nativeObject = &gNear, .ops = &ops, .objectType = "near", .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL};
	assert(PangeaScript_RegisterObject(&registration,&gNearHandle) == PANGEA_SCRIPT_OK);
	registration.nativeObject = &gFar; registration.objectType = "far";
	assert(PangeaScript_RegisterObject(&registration,&gFarHandle) == PANGEA_SCRIPT_OK);
	gMover = (Collider){.bounds = {.min = {-.5f,-.5f,-.5f}, .max = {.5f,.5f,.5f}}, .available = true};
	registration.nativeObject = &gMover; registration.objectType = "mover";
	assert(PangeaScript_RegisterObject(&registration,&gMoverHandle) == PANGEA_SCRIPT_OK);
	test_rays(); test_sweeps(); test_kinematic_movement(); test_lua_geometry(&game); test_2d_and_diagonal(); test_incomplete_queries();
	PangeaScript_Shutdown();
	puts("Geometry query tests passed");
	return 0;
}
