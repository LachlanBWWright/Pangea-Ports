#include "pangea_presentation.h"
#include "pangea_presentation_layout.h"
#include "pangea_script.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define RESOURCE_COUNT 32
typedef enum ResourceKind { MESSAGE, OBJECTIVE, VOICE, EMITTER } ResourceKind;
typedef struct Resource
{
	bool active;
	uint32_t generation;
	ResourceKind kind;
	char id[PANGEA_PRESENTATION_ID];
	char text[PANGEA_PRESENTATION_TEXT];
	PangeaPresentationOptions options;
	float elapsed;
	float nextEmission;
	int voice;
} Resource;

static Resource gResources[RESOURCE_COUNT];
static PangeaPresentationNative gNative;
static uint32_t gGeneration;

PangeaPresentationOptions PangeaPresentation_DefaultOptions(void)
{
	return (PangeaPresentationOptions){.duration=4, .volume=1, .pitch=1, .scale=1, .player=-1};
}

const PangeaPresentationNative* PangeaPresentation_GetNative(void) { return &gNative; }

void PangeaPresentation_Configure(const PangeaPresentationNative* native)
{
	PangeaPresentation_Reset();
	gNative = native ? *native : (PangeaPresentationNative){0};
}

static bool valid_options(const PangeaPresentationOptions* options)
{
	int players=gNative.activePlayers ? gNative.activePlayers() : gNative.players;
	if (players<=0) players=1;
	if (players>PANGEA_PRESENTATION_PLAYERS) players=PANGEA_PRESENTATION_PLAYERS;
	return options && isfinite(options->duration) && options->duration >= 0 && options->duration <= 3600 &&
		isfinite(options->volume) && options->volume >= 0 && options->volume <= 1 &&
		isfinite(options->pitch) && options->pitch >= .25f && options->pitch <= 4 &&
		isfinite(options->scale) && options->scale > 0 && options->scale <= 10 &&
		isfinite(options->x) && isfinite(options->y) && isfinite(options->z) &&
		options->player >= -1 && options->player < players && options->priority >= 0 && options->priority <= 3 &&
		(!options->hasOwner || PangeaScript_ObjectExists((PangeaScriptObjectHandle){options->ownerID, options->ownerGeneration}));
}

bool PangeaPresentation_OptionsValid(const PangeaPresentationOptions* options) { return valid_options(options); }

static bool valid_text(const char* text, size_t capacity)
{
	return text && text[0] && strlen(text) < capacity && PangeaPresentation_ValidUTF8(text,strlen(text));
}

static Resource* find_resource(PangeaPresentationHandle handle)
{
	if (!handle.id || handle.id > RESOURCE_COUNT) return NULL;
	Resource* resource = &gResources[handle.id-1];
	return resource->active && resource->generation == handle.generation ? resource : NULL;
}

static Resource* allocate(ResourceKind kind, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle)
{
	int kindCount = 0;
	for (int i=0; i<RESOURCE_COUNT; i++)
		if (gResources[i].active && gResources[i].kind == kind) kindCount++;
	if (kindCount >= 8) return NULL;
	for (int i=0; i<RESOURCE_COUNT; i++)
	{
		if (gResources[i].active) continue;
		if (++gGeneration == 0) ++gGeneration;
		gResources[i] = (Resource){.active=true, .generation=gGeneration, .kind=kind, .options=*options, .voice=-1};
		*handle = (PangeaPresentationHandle){(uint32_t)i+1, gGeneration};
		return &gResources[i];
	}
	return NULL;
}

static void release(Resource* resource)
{
	if (resource->kind == VOICE && resource->voice >= 0 && gNative.stop) gNative.stop(resource->voice);
	resource->active = false;
}

PangeaPresentationStatus PangeaPresentation_Message(const char* text, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle)
{
	if (!handle || !valid_options(options) || !valid_text(text, PANGEA_PRESENTATION_TEXT)) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	if (!gNative.text) return PANGEA_PRESENTATION_UNSUPPORTED;
	if (gNative.textAvailable && !gNative.textAvailable()) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	if (options->modal) return PANGEA_PRESENTATION_UNSUPPORTED;
	Resource* resource = allocate(MESSAGE, options, handle);
	if (!resource) return PANGEA_PRESENTATION_CAPACITY;
	snprintf(resource->text, sizeof(resource->text), "%s", text);
	return PANGEA_PRESENTATION_OK;
}

PangeaPresentationStatus PangeaPresentation_Objective(const char* id, const char* text, int current, int total, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle)
{
	if (!handle || !valid_options(options) || !valid_text(id, PANGEA_PRESENTATION_ID) ||
		!valid_text(text, PANGEA_PRESENTATION_TEXT-40) || current < 0 || total < current) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	if (!gNative.text) return PANGEA_PRESENTATION_UNSUPPORTED;
	if (gNative.textAvailable && !gNative.textAvailable()) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	if (options->modal) return PANGEA_PRESENTATION_UNSUPPORTED;
	Resource* resource = NULL;
	for (int i=0; i<RESOURCE_COUNT; i++)
	{
		if (!gResources[i].active || gResources[i].kind != OBJECTIVE || strcmp(gResources[i].id,id)) continue;
		resource = &gResources[i];
		*handle = (PangeaPresentationHandle){(uint32_t)i+1, resource->generation};
		break;
	}
	if (!resource) resource = allocate(OBJECTIVE, options, handle);
	if (!resource) return PANGEA_PRESENTATION_CAPACITY;
	resource->options = *options;
	snprintf(resource->id, sizeof(resource->id), "%s", id);
	if (total) snprintf(resource->text, sizeof(resource->text), "%s (%d/%d)", text, current, total);
	else snprintf(resource->text, sizeof(resource->text), "%s", text);
	return PANGEA_PRESENTATION_OK;
}

PangeaPresentationStatus PangeaPresentation_Play(const char* id, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle)
{
	if (!handle || !valid_options(options) || !valid_text(id,PANGEA_PRESENTATION_ID)) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	if (!gNative.play || options->loop || (options->positional && !gNative.positionalAudio)) return PANGEA_PRESENTATION_UNSUPPORTED;
	Resource* resource = allocate(VOICE, options, handle);
	if (!resource) return PANGEA_PRESENTATION_CAPACITY;
	PangeaPresentationStatus status = gNative.play(id,options,&resource->voice);
	if (status != PANGEA_PRESENTATION_OK) resource->active = false;
	return status;
}

PangeaPresentationStatus PangeaPresentation_Effect(const char* id, const PangeaPresentationOptions* options, bool attached, PangeaPresentationHandle* handle)
{
	if (!handle || !valid_options(options) || !valid_text(id,PANGEA_PRESENTATION_ID) || (attached && !options->hasOwner)) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	if (!gNative.burst || (attached && !gNative.attachedEffects)) return PANGEA_PRESENTATION_UNSUPPORTED;
	if (!attached) { *handle=(PangeaPresentationHandle){0}; return gNative.burst(id,options); }
	Resource* resource = allocate(EMITTER,options,handle);
	if (!resource) return PANGEA_PRESENTATION_CAPACITY;
	snprintf(resource->id,sizeof(resource->id),"%s",id);
	PangeaPresentationStatus status = gNative.burst(id,options);
	if (status != PANGEA_PRESENTATION_OK) resource->active=false;
	resource->nextEmission=.2f;
	return status;
}

PangeaPresentationStatus PangeaPresentation_Remove(PangeaPresentationHandle handle)
{
	Resource* resource = find_resource(handle);
	if (!resource) return PANGEA_PRESENTATION_STALE_HANDLE;
	release(resource);
	return PANGEA_PRESENTATION_OK;
}

bool PangeaPresentation_Active(PangeaPresentationHandle handle) { return find_resource(handle) != NULL; }

void PangeaPresentation_ClearOwner(int id, uint32_t generation)
{
	for (int i=0; i<RESOURCE_COUNT; i++)
	{
		Resource* resource=&gResources[i];
		if (resource->active && resource->options.hasOwner && resource->options.ownerID==id && resource->options.ownerGeneration==generation) release(resource);
	}
	PangeaDialogue_ClearOwner(id,generation);
}

void PangeaPresentation_Reset(void)
{
	for (int i=0; i<RESOURCE_COUNT; i++) if (gResources[i].active) release(&gResources[i]);
	PangeaDialogue_Reset();
	if (gNative.reset) gNative.reset();
}

void PangeaPresentation_Update(float delta)
{
	if (!isfinite(delta) || delta < 0) return;
	for (int i=0; i<RESOURCE_COUNT; i++)
	{
		Resource* resource=&gResources[i];
		if (!resource->active) continue;
		resource->elapsed += delta;
		if (resource->options.hasOwner)
		{
			PangeaScriptVector3 position;
			if (!PangeaScript_GetObjectPosition((PangeaScriptObjectHandle){resource->options.ownerID,resource->options.ownerGeneration},&position)) { release(resource); continue; }
			resource->options.x=position.x; resource->options.y=position.y; resource->options.z=position.z;
		}
		if (resource->kind==VOICE && gNative.playing && !gNative.playing(resource->voice)) { resource->active=false; continue; }
		if (resource->kind!=OBJECTIVE && resource->kind!=VOICE && resource->options.duration > 0 && resource->elapsed>=resource->options.duration) { release(resource); continue; }
		if (resource->kind==VOICE && resource->options.positional && gNative.position) gNative.position(resource->voice,resource->options.x,resource->options.y,resource->options.z);
		if (resource->kind==EMITTER && resource->elapsed>=resource->nextEmission)
		{
			resource->nextEmission=resource->elapsed+.2f;
			if (gNative.burst(resource->id,&resource->options)!=PANGEA_PRESENTATION_OK) release(resource);
		}
	}
	PangeaDialogue_Update(delta);
	if (gNative.input) gNative.input();
}

static int append_text(PangeaPresentationRow* rows, int capacity, const Resource* resource)
{
	const char* text=resource->text;
	int count=0;
	while (*text && count<capacity)
	{
		rows[count]=(PangeaPresentationRow){.player=resource->options.player,.priority=resource->options.priority};
		text+=PangeaPresentation_WrapLine(text,rows[count++].text);
	}
	return count;
}

int PangeaPresentation_Rows(PangeaPresentationRow* rows, int capacity)
{
	if (!rows || capacity<=0) return 0;
	int count=PangeaDialogue_Rows(rows,capacity);
	for (int priority=3; priority>=0 && count<capacity; priority--)
		for (int i=0; i<RESOURCE_COUNT && count<capacity; i++)
			if (gResources[i].active && gResources[i].kind<=OBJECTIVE && gResources[i].options.priority==priority)
				count+=append_text(rows+count,capacity-count,&gResources[i]);
	return count;
}

int PangeaPresentation_RowsForPlayer(PangeaPresentationRow* rows, int capacity, int player)
{
	if (!rows || capacity<=0 || player<0 || player>=PANGEA_PRESENTATION_PLAYERS) return 0;
	int count=PangeaDialogue_RowsForPlayer(rows,capacity,player);
	for (int priority=3; priority>=0 && count<capacity; priority--)
		for (int i=0; i<RESOURCE_COUNT && count<capacity; i++)
		{
			const Resource* resource=&gResources[i];
			if (!resource->active || resource->kind>OBJECTIVE || resource->options.priority!=priority) continue;
			if (resource->options.player>=0 && resource->options.player!=player) continue;
			count+=append_text(rows+count,capacity-count,resource);
		}
	return count;
}
