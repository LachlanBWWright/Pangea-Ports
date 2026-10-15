#include "pangea_presentation.h"
#include "pangea_presentation_layout.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEFINITIONS 16
#define EVENTS 32
static PangeaDialogueDefinition gDefinitions[DEFINITIONS];
static PangeaDialogueEvent gEvents[EVENTS];
static int gEventCount;
static uint32_t gGeneration;
typedef struct DialogueSession
{
	bool active;
	PangeaPresentationHandle handle;
	PangeaPresentationHandle voice;
	PangeaPresentationOptions options;
	int definition;
	int node;
	int selected;
	float elapsed;
	size_t offset;
} DialogueSession;
static DialogueSession gSessions[PANGEA_PRESENTATION_PLAYERS];

static void event(DialogueSession* session, const char* reason, const char* choice)
{
	if (gEventCount==EVENTS) return;
	PangeaDialogueEvent* output=&gEvents[gEventCount++];
	*output=(PangeaDialogueEvent){.handle=session->handle,.player=session->options.player};
	snprintf(output->id,sizeof(output->id),"%s",gDefinitions[session->definition].id);
	snprintf(output->reason,sizeof(output->reason),"%s",reason);
	snprintf(output->choice,sizeof(output->choice),"%s",choice ? choice : "");
}

static void stop_voice(DialogueSession* session)
{
	if (session->voice.id) (void)PangeaPresentation_Remove(session->voice);
	session->voice=(PangeaPresentationHandle){0};
}

static void finish(DialogueSession* session)
{
	stop_voice(session);
	if (!session->active) return;
	session->active=false;
	const PangeaPresentationNative* native=PangeaPresentation_GetNative();
	if (native->endDialogue) native->endDialogue();
}

static void enter_node(DialogueSession* session, int node)
{
	stop_voice(session);
	if (node<0 || node>=gDefinitions[session->definition].nodeCount)
	{
		event(session,"complete",NULL);
		finish(session);
		return;
	}
	session->node=node;
	session->elapsed=0;
	session->offset=0;
	session->selected=0;
	const PangeaDialogueNode* current=&gDefinitions[session->definition].nodes[node];
	if (current->voice[0]) (void)PangeaPresentation_Play(current->voice,&session->options,&session->voice);
}

PangeaPresentationStatus PangeaDialogue_Define(const PangeaDialogueDefinition* definition)
{
	if (!definition || !definition->id[0] || definition->nodeCount<=0 || definition->nodeCount>PANGEA_PRESENTATION_NODES) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	for (int i=0; i<definition->nodeCount; i++)
	{
		const PangeaDialogueNode* node=&definition->nodes[i];
		if (!node->text[0] || !isfinite(node->duration) || node->duration<0 || node->duration>3600 ||
			node->next < -1 || node->next>=definition->nodeCount || node->choiceCount<0 || node->choiceCount>PANGEA_PRESENTATION_CHOICES) return PANGEA_PRESENTATION_BAD_ARGUMENT;
		for (int choice=0; choice<node->choiceCount; choice++)
			if (!node->choices[choice].id[0] || !node->choices[choice].text[0] || node->choices[choice].next < -1 || node->choices[choice].next>=definition->nodeCount) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	}
	int empty=-1;
	for (int i=0; i<DEFINITIONS; i++)
	{
		if (!gDefinitions[i].id[0] && empty<0) empty=i;
		if (strcmp(gDefinitions[i].id,definition->id)) continue;
		for (int player=0; player<PANGEA_PRESENTATION_PLAYERS; player++)
			if (gSessions[player].active && gSessions[player].definition==i) return PANGEA_PRESENTATION_CAPACITY;
		gDefinitions[i]=*definition;
		return PANGEA_PRESENTATION_OK;
	}
	if (empty<0) return PANGEA_PRESENTATION_CAPACITY;
	gDefinitions[empty]=*definition;
	return PANGEA_PRESENTATION_OK;
}

PangeaPresentationStatus PangeaDialogue_Start(const char* id, const PangeaPresentationOptions* options, PangeaPresentationHandle* handle)
{
	const PangeaPresentationNative* native=PangeaPresentation_GetNative();
	if (!id || !handle || !PangeaPresentation_OptionsValid(options)) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	if (options->modal) return PANGEA_PRESENTATION_UNSUPPORTED;
	if (!native->text) return PANGEA_PRESENTATION_UNSUPPORTED;
	if (native->textAvailable && !native->textAvailable()) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	int player=options->player<0 ? 0 : options->player;
	if (player>=PANGEA_PRESENTATION_PLAYERS) return PANGEA_PRESENTATION_BAD_ARGUMENT;
	DialogueSession* session=&gSessions[player];
	if (session->active) return PANGEA_PRESENTATION_CAPACITY;
	for (int i=0; i<DEFINITIONS; i++)
	{
		if (strcmp(gDefinitions[i].id,id)) continue;
		for (int n=0; n<gDefinitions[i].nodeCount; n++)
			if (gDefinitions[i].nodes[n].choiceCount && !native->choices) return PANGEA_PRESENTATION_UNSUPPORTED;
		if (native->beginDialogue && !native->beginDialogue()) return PANGEA_PRESENTATION_CAPACITY;
		if (++gGeneration==0) ++gGeneration;
		*session=(DialogueSession){.active=true,.definition=i,.options=*options,.handle={1000+(uint32_t)player,gGeneration}};
		session->options.player=player;
		*handle=session->handle;
		enter_node(session,0);
		return PANGEA_PRESENTATION_OK;
	}
	return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
}

static DialogueSession* find_session(PangeaPresentationHandle handle)
{
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS; i++)
		if (gSessions[i].active && handle.id==gSessions[i].handle.id && handle.generation==gSessions[i].handle.generation) return &gSessions[i];
	return NULL;
}

bool PangeaDialogue_Active(PangeaPresentationHandle handle) { return find_session(handle)!=NULL; }

PangeaPresentationStatus PangeaDialogue_Cancel(PangeaPresentationHandle handle)
{
	DialogueSession* session=find_session(handle);
	if (!session) return PANGEA_PRESENTATION_STALE_HANDLE;
	event(session,"cancelled",NULL);
	finish(session);
	return PANGEA_PRESENTATION_OK;
}

void PangeaDialogue_Reset(void)
{
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS; i++) finish(&gSessions[i]);
	gEventCount=0;
}

int PangeaDialogue_Player(void)
{
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS; i++) if (gSessions[i].active) return i;
	return 0;
}

bool PangeaDialogue_IsActiveForPlayer(int player)
{
	return player>=0 && player<PANGEA_PRESENTATION_PLAYERS && gSessions[player].active;
}

void PangeaDialogue_ClearDefinitions(void)
{
	PangeaDialogue_Reset();
	memset(gDefinitions,0,sizeof(gDefinitions));
}

void PangeaDialogue_ClearOwner(int id, uint32_t generation)
{
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS; i++)
		if (gSessions[i].active && gSessions[i].options.hasOwner && gSessions[i].options.ownerID==id && gSessions[i].options.ownerGeneration==generation)
			(void)PangeaDialogue_Cancel(gSessions[i].handle);
}

void PangeaDialogue_Update(float delta)
{
	if (!isfinite(delta) || delta<0) return;
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS; i++)
	{
		DialogueSession* session=&gSessions[i];
		if (!session->active) continue;
		if (!PangeaPresentation_OptionsValid(&session->options))
		{
			(void)PangeaDialogue_Cancel(session->handle);
			continue;
		}
		session->elapsed+=delta;
		const PangeaDialogueNode* node=&gDefinitions[session->definition].nodes[session->node];
		if (!node->choiceCount && node->duration>0 && session->elapsed>=node->duration && !PangeaPresentation_Active(session->voice)) PangeaDialogue_InputForPlayer(i,true,false,0);
	}
}

static size_t page_end(const DialogueSession* session, const PangeaDialogueNode* node)
{
	size_t offset=session->offset;
	char line[128];
	for (int row=0; row<6 && node->text[offset]; row++) offset+=PangeaPresentation_WrapLine(node->text+offset,line);
	return offset;
}

void PangeaDialogue_Input(bool confirm, bool back, int direction)
{
	PangeaDialogue_InputForPlayer(PangeaDialogue_Player(),confirm,back,direction);
}

void PangeaDialogue_InputForPlayer(int player, bool confirm, bool back, int direction)
{
	if (!PangeaDialogue_IsActiveForPlayer(player)) return;
	DialogueSession* session=&gSessions[player];
	if (back) { (void)PangeaDialogue_Cancel(session->handle); return; }
	const PangeaDialogueNode* node=&gDefinitions[session->definition].nodes[session->node];
	size_t nextPage=page_end(session,node);
	if (node->choiceCount && !node->text[nextPage]) session->selected=(session->selected+(direction>0 ? 1 : direction<0 ? -1 : 0)+node->choiceCount)%node->choiceCount;
	if (!confirm || session->elapsed<.2f) return;
	if (node->text[nextPage]) { session->offset=nextPage; session->elapsed=0; return; }
	if (!node->choiceCount) { enter_node(session,node->next); return; }
	const PangeaDialogueChoice* choice=&node->choices[session->selected];
	event(session,"choice",choice->id);
	enter_node(session,choice->next);
}

int PangeaDialogue_RowsForPlayer(PangeaPresentationRow* rows, int capacity, int player)
{
	if (!PangeaDialogue_IsActiveForPlayer(player) || !rows || capacity<=0) return 0;
	const DialogueSession* session=&gSessions[player];
	const PangeaDialogueNode* node=&gDefinitions[session->definition].nodes[session->node];
	int count=0;
	if (node->speaker[0])
	{
		rows[count]=(PangeaPresentationRow){.player=player,.dialogue=true};
		snprintf(rows[count++].text,128,"%s:",node->speaker);
	}
	const char* text=node->text+session->offset;
	int bodyRows=0;
	while (*text && count<capacity && bodyRows++<6)
	{
		rows[count]=(PangeaPresentationRow){.player=player,.dialogue=true};
		text+=PangeaPresentation_WrapLine(text,rows[count++].text);
	}
	if (*text && count<capacity)
	{
		rows[count]=(PangeaPresentationRow){.player=player,.dialogue=true};
		snprintf(rows[count++].text,128,"Confirm to continue...");
		return count;
	}
	for (int i=0; i<node->choiceCount && count<capacity; i++)
	{
		rows[count]=(PangeaPresentationRow){.player=player,.dialogue=true};
		snprintf(rows[count++].text,128,"%s %.120s",i==session->selected ? ">" : " ",node->choices[i].text);
	}
	return count;
}

int PangeaDialogue_Rows(PangeaPresentationRow* rows, int capacity)
{
	if (!rows || capacity<=0) return 0;
	int count=0;
	for (int i=0; i<PANGEA_PRESENTATION_PLAYERS && count<capacity; i++) count+=PangeaDialogue_RowsForPlayer(rows+count,capacity-count,i);
	return count;
}

bool PangeaDialogue_Poll(PangeaDialogueEvent* output)
{
	if (!output || !gEventCount) return false;
	*output=gEvents[0];
	gEventCount--;
	memmove(gEvents,gEvents+1,(size_t)gEventCount*sizeof(gEvents[0]));
	return true;
}
