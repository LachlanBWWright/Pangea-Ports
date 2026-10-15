#include "../shared/script/pangea_script_backend.h"
#include "../shared/script/pangea_presentation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int gPlays, gStops, gBursts;
static bool gPlaying;
static int gDialogueBegins, gDialogueEnds, gActivePlayers=4;
static bool gAllowDialogue=true;
static bool begin_dialogue(void) { gDialogueBegins++; return gAllowDialogue; }
static void end_dialogue(void) { gDialogueEnds++; }
static int active_players(void) { return gActivePlayers; }
static const char* const kSounds[]={"pickup.chime"};
static const char* const kEffects[]={"sparkles"};

static PangeaPresentationStatus play(const char* id, const PangeaPresentationOptions* options, int* voice)
{
	if (strcmp(id,"pickup.chime")) return PANGEA_PRESENTATION_ASSET_NOT_LOADED;
	assert(options->volume==1);
	*voice=7; gPlays++; gPlaying=true;
	return PANGEA_PRESENTATION_OK;
}

static void stop(int voice) { assert(voice==7); gStops++; gPlaying=false; }
static bool playing(int voice) { assert(voice==7); return gPlaying; }
static PangeaPresentationStatus burst(const char* id, const PangeaPresentationOptions* options)
{
	assert(!strcmp(id,"sparkles") && options->scale==1); gBursts++;
	return PANGEA_PRESENTATION_OK;
}

static void test_ui(void)
{
	PangeaPresentationOptions options=PangeaPresentation_DefaultOptions();
	PangeaPresentationHandle message, objective, updated;
	assert(PangeaPresentation_Message("A message",&options,&message)==PANGEA_PRESENTATION_OK);
	assert(PangeaPresentation_Objective("rescue","Rescue friends",1,3,&options,&objective)==PANGEA_PRESENTATION_OK);
	assert(PangeaPresentation_Objective("rescue","Rescue friends",2,3,&options,&updated)==PANGEA_PRESENTATION_OK);
	assert(updated.id==objective.id && updated.generation==objective.generation);
	PangeaPresentation_Update(5);
	assert(!PangeaPresentation_Active(message));
	assert(PangeaPresentation_Active(objective));
	PangeaPresentationRow rows[12];
	assert(PangeaPresentation_Rows(rows,12)==1 && strstr(rows[0].text,"2/3"));
	PangeaPresentation_Reset();
	assert(PangeaPresentation_Remove(objective)==PANGEA_PRESENTATION_STALE_HANDLE);
	assert(PangeaPresentation_Message("Replacement",&options,&updated)==PANGEA_PRESENTATION_OK);
	assert(updated.generation!=message.generation);
	PangeaPresentation_Reset();
	for (int i=0; i<8; i++) assert(PangeaPresentation_Message("bounded",&options,&updated)==PANGEA_PRESENTATION_OK);
	assert(PangeaPresentation_Message("overflow",&options,&updated)==PANGEA_PRESENTATION_CAPACITY);
	PangeaPresentation_Reset();
}

static void test_dialogue(void)
{
	PangeaDialogueDefinition definition={.id="quest",.nodeCount=2};
	definition.nodes[0]=(PangeaDialogueNode){.text="Will you help?",.speaker="Guide",.duration=1,.next=-1,.choiceCount=1};
	definition.nodes[0].choices[0]=(PangeaDialogueChoice){.id="accept",.text="Yes",.next=1};
	definition.nodes[1]=(PangeaDialogueNode){.text="Thank you",.duration=1,.next=-1};
	assert(PangeaDialogue_Define(&definition)==PANGEA_PRESENTATION_OK);
	PangeaPresentationOptions options=PangeaPresentation_DefaultOptions();
	PangeaPresentationHandle handle;
	assert(PangeaDialogue_Start("quest",&options,&handle)==PANGEA_PRESENTATION_OK);
	PangeaPresentation_Update(2);
	assert(PangeaDialogue_Active(handle));
	PangeaDialogue_Input(true,false,0);
	PangeaDialogueEvent event;
	assert(PangeaDialogue_Poll(&event) && !strcmp(event.choice,"accept") && !strcmp(event.reason,"choice"));
	PangeaPresentation_Update(2);
	assert(!PangeaDialogue_Active(handle));
	assert(PangeaDialogue_Poll(&event) && !strcmp(event.reason,"complete"));
	assert(!PangeaDialogue_Poll(&event));
	assert(PangeaDialogue_Start("missing",&options,&handle)==PANGEA_PRESENTATION_ASSET_NOT_LOADED);
	definition.nodes[1].next=9;
	assert(PangeaDialogue_Define(&definition)==PANGEA_PRESENTATION_BAD_ARGUMENT);
}

static void test_dialogue_pagination(void)
{
	PangeaDialogueDefinition definition={.id="long",.nodeCount=1};
	PangeaDialogueNode* node=&definition.nodes[0];
	memset(node->text,'x',400); node->text[400]=0;
	node->next=-1; node->choiceCount=1;
	node->choices[0]=(PangeaDialogueChoice){.id="finish",.text="Finish",.next=-1};
	assert(PangeaDialogue_Define(&definition)==PANGEA_PRESENTATION_OK);
	PangeaPresentationOptions options=PangeaPresentation_DefaultOptions(); PangeaPresentationHandle handle;
	assert(PangeaDialogue_Start("long",&options,&handle)==PANGEA_PRESENTATION_OK);
	PangeaPresentationRow rows[12];
	int count=PangeaDialogue_Rows(rows,12);
	assert(count>0 && !strcmp(rows[count-1].text,"Confirm to continue..."));
	PangeaPresentation_Update(1); PangeaDialogue_Input(true,false,0);
	assert(PangeaDialogue_Active(handle));
	count=PangeaDialogue_Rows(rows,12);
	assert(!strcmp(rows[count-1].text,"> Finish"));
	PangeaPresentation_Update(1); PangeaDialogue_Input(true,false,0);
	assert(!PangeaDialogue_Active(handle));
	PangeaPresentation_Reset();
}

static void test_lua(PangeaScriptBackend* backend)
{
	char error[1024]={0};
	const char* source=
		"assert(pangea.api.capabilities().presentation.text) "
		"assert(not pangea.audio.play('pickup.chime',{position={x=1,y=2,z=3}}).ok) "
		"assert(not pangea.ui.message('bad',{duration=-1}).ok) "
		"assert(not pangea.ui.message(string.rep('x',512)).ok) "
		"assert(not pangea.ui.message('bad',{duration='3'}).ok) "
		"local ui=pangea.ui.message('Hello') assert(ui.ok and pangea.ui.isActive(ui.handle)) "
		"assert(pangea.ui.remove(ui.handle).ok) assert(not pangea.ui.remove(ui.handle).ok) "
		"assert(pangea.audio.play('pickup.chime').ok) "
		"assert(pangea.effects.burst('sparkles').ok) "
		"assert(pangea.dialogue.define('intro',{{speaker='Guide',text='Welcome',duration=.1}}).ok) "
		"assert(pangea.dialogue.start('intro').ok) "
		"return {onDialogueResult=function(ctx) assert(ctx.dialogueId=='intro' and ctx.reason=='complete' and ctx.playerNum==0) end}";
	assert(PangeaScriptBackend_Load(backend,source,error,sizeof(error))==PANGEA_SCRIPT_OK);
	PangeaScriptFrameContext frame={.deltaSeconds=.2f,.levelTimeSeconds=.2f};
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(gPlays==1 && gBursts==1);
	PangeaPresentation_Reset();
	assert(gStops==1);
}

static void test_player_dialogue(void)
{
	PangeaPresentationNative native=*PangeaPresentation_GetNative();
	native.players=4; native.activePlayers=active_players;
	native.beginDialogue=begin_dialogue; native.endDialogue=end_dialogue;
	PangeaPresentation_Configure(&native);
	PangeaDialogueDefinition definition={.id="players",.nodeCount=1};
	definition.nodes[0]=(PangeaDialogueNode){.text="Choose for your player",.next=-1,.choiceCount=1};
	definition.nodes[0].choices[0]=(PangeaDialogueChoice){.id="done",.text="Done",.next=-1};
	assert(PangeaDialogue_Define(&definition)==PANGEA_PRESENTATION_OK);
	PangeaPresentationOptions options=PangeaPresentation_DefaultOptions();
	PangeaPresentationHandle handles[4], duplicate;
	for (int i=0; i<4; i++)
	{
		options.player=i;
		assert(PangeaDialogue_Start("players",&options,&handles[i])==PANGEA_PRESENTATION_OK);
		assert(PangeaDialogue_IsActiveForPlayer(i));
	}
	assert(gDialogueBegins==4 && gDialogueEnds==0);
	assert(PangeaDialogue_Start("players",&options,&duplicate)==PANGEA_PRESENTATION_CAPACITY);
	assert(PangeaDialogue_Define(&definition)==PANGEA_PRESENTATION_CAPACITY);
	options.player=-1;
	PangeaPresentationHandle message;
	assert(PangeaPresentation_Message("Global message",&options,&message)==PANGEA_PRESENTATION_OK);
	PangeaPresentation_Update(.3f);
	PangeaDialogue_InputForPlayer(2,true,false,0);
	assert(!PangeaDialogue_Active(handles[2]) && gDialogueEnds==1);
	for (int i=0; i<4; i++)
	{
		PangeaPresentationRow rows[12];
		int count=PangeaPresentation_RowsForPlayer(rows,12,i);
		assert(count==(i==2 ? 1 : 3));
		assert(!strcmp(rows[count-1].text,"Global message"));
		if (i!=2) assert(rows[0].player==i && rows[0].dialogue && PangeaDialogue_Active(handles[i]));
	}
	PangeaDialogueEvent event;
	assert(PangeaDialogue_Poll(&event) && event.player==2 && event.handle.id==handles[2].id && !strcmp(event.reason,"choice"));
	assert(PangeaDialogue_Poll(&event) && event.handle.id==handles[2].id && !strcmp(event.reason,"complete"));
	PangeaPresentation_Reset();
	assert(gDialogueEnds==4);
	PangeaPresentation_Reset();
	assert(gDialogueEnds==4);
	gActivePlayers=1; options.player=1;
	assert(PangeaDialogue_Start("players",&options,&duplicate)==PANGEA_PRESENTATION_BAD_ARGUMENT);
	options.player=0; gAllowDialogue=false;
	assert(PangeaDialogue_Start("players",&options,&duplicate)==PANGEA_PRESENTATION_CAPACITY);
	assert(!PangeaDialogue_IsActiveForPlayer(0) && gDialogueEnds==4);
	gAllowDialogue=true; gActivePlayers=4;
	options.player=3;
	assert(PangeaDialogue_Start("players",&options,&duplicate)==PANGEA_PRESENTATION_OK);
	gActivePlayers=1;
	PangeaPresentation_Update(.1f);
	assert(!PangeaDialogue_Active(duplicate));
	assert(PangeaDialogue_Poll(&event) && event.player==3 && !strcmp(event.reason,"cancelled"));
	gActivePlayers=4;
	native.players=1; native.activePlayers=NULL; native.beginDialogue=NULL; native.endDialogue=NULL;
	PangeaPresentation_Configure(&native);
}

static void test_containment(PangeaScriptBackend* backend)
{
	char error[1024]={0}; PangeaScriptFrameContext frame={.deltaSeconds=.1f,.levelTimeSeconds=1};
	assert(PangeaScriptBackend_Load(backend,"pangea.events.on('nested',function() end) return {onFrame=function() pangea.events.emit('nested') while true do end end}",error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_BUDGET_EXCEEDED);
	assert(PangeaScriptBackend_Load(backend,"pangea.events.on('empty',function(payload) assert(payload==nil) end) return {onFrame=function() pangea.events.emit('empty') end}",error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_Load(backend,"return setmetatable({}, {__index=function() error('unprotected lookup') end})",error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_Load(backend,"local started=false return {onFrame=function() if not started then started=true pangea.time.every(1e-30,function() end) end end}",error,sizeof(error))==PANGEA_SCRIPT_OK);
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_OK);
	frame.levelTimeSeconds=2;
	assert(PangeaScriptBackend_CallFrameHook(backend,&frame,error,sizeof(error))==PANGEA_SCRIPT_OK);
}

int main(void)
{
	const PangeaPresentationNative native={.text=true,.choices=true,.play=play,.stop=stop,.playing=playing,.burst=burst,.sounds=kSounds,.soundCount=1,.effects=kEffects,.effectCount=1};
	PangeaPresentation_Configure(&native);
	test_ui(); test_dialogue(); test_dialogue_pagination(); test_player_dialogue();
	const PangeaScriptGameInfo info={.gameId="OttoMatic-Android",.gameName="Otto Matic"};
	PangeaScriptBackend* backend=PangeaScriptBackend_Create(&info);
	assert(backend);
	test_lua(backend); test_containment(backend);
	PangeaScriptBackend_Destroy(backend);
	PangeaPresentation_Configure(NULL);
	PangeaPresentationOptions options=PangeaPresentation_DefaultOptions(); PangeaPresentationHandle handle;
	assert(PangeaPresentation_Message("unsupported",&options,&handle)==PANGEA_PRESENTATION_UNSUPPORTED);
	puts("Presentation and callback containment tests passed");
	return 0;
}
