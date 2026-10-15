#include "pangea_presentation.h"
#include "pangea_presentation_lua.h"
#include "pangea_presentation_layout.h"
#include <lua.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void field(lua_State* lua, int table, const char* name)
{
	table=lua_absindex(lua,table);
	lua_pushstring(lua,name);
	lua_rawget(lua,table);
}

static bool string_value(lua_State* lua, int index, char* output, size_t capacity, bool optional)
{
	if (optional && lua_isnil(lua,index)) { output[0]=0; return true; }
	if (lua_type(lua,index)!=LUA_TSTRING) return false;
	size_t size=0;
	const char* value=lua_tolstring(lua,index,&size);
	if (size>=capacity || memchr(value,0,size) || !PangeaPresentation_ValidUTF8(value,size)) return false;
	memcpy(output,value,size); output[size]=0;
	return optional || size>0;
}

static bool string_field(lua_State* lua, int table, const char* name, char* output, size_t capacity, bool optional)
{
	field(lua,table,name);
	bool valid=string_value(lua,-1,output,capacity,optional);
	lua_pop(lua,1);
	return valid;
}

static bool number_field(lua_State* lua, int table, const char* name, float* output)
{
	field(lua,table,name);
	bool valid=lua_isnil(lua,-1) || (lua_type(lua,-1)==LUA_TNUMBER && isfinite(lua_tonumber(lua,-1)) && fabs(lua_tonumber(lua,-1))<=3.4e38);
	if (valid && !lua_isnil(lua,-1)) *output=(float)lua_tonumber(lua,-1);
	lua_pop(lua,1);
	return valid;
}

static bool integer_field(lua_State* lua, int table, const char* name, int* output)
{
	field(lua,table,name);
	bool valid=lua_isnil(lua,-1) || (lua_isinteger(lua,-1) && lua_tointeger(lua,-1)>=INT_MIN && lua_tointeger(lua,-1)<=INT_MAX);
	if (valid && !lua_isnil(lua,-1)) *output=(int)lua_tointeger(lua,-1);
	lua_pop(lua,1);
	return valid;
}

static bool bool_field(lua_State* lua, int table, const char* name, bool* output)
{
	field(lua,table,name);
	bool valid=lua_isnil(lua,-1) || lua_isboolean(lua,-1);
	if (valid && !lua_isnil(lua,-1)) *output=lua_toboolean(lua,-1);
	lua_pop(lua,1);
	return valid;
}

static bool handle_value(lua_State* lua, int index, PangeaPresentationHandle* handle)
{
	if (!lua_istable(lua,index)) return false;
	int id=0, generation=0;
	if (!integer_field(lua,index,"id",&id) || !integer_field(lua,index,"generation",&generation) || id<=0 || generation<=0) return false;
	*handle=(PangeaPresentationHandle){(uint32_t)id,(uint32_t)generation};
	return true;
}

static bool options_value(lua_State* lua, int index, PangeaPresentationOptions* options)
{
	*options=PangeaPresentation_DefaultOptions();
	if (lua_isnoneornil(lua,index)) return true;
	if (!lua_istable(lua,index)) return false;
	bool valid=number_field(lua,index,"duration",&options->duration) && number_field(lua,index,"volume",&options->volume) &&
		number_field(lua,index,"pitch",&options->pitch) && number_field(lua,index,"scale",&options->scale) &&
		integer_field(lua,index,"player",&options->player) && integer_field(lua,index,"priority",&options->priority) && bool_field(lua,index,"loop",&options->loop) && bool_field(lua,index,"modal",&options->modal);
	field(lua,index,"position");
	if (!lua_isnil(lua,-1))
	{
		options->positional=true;
		valid=valid && lua_istable(lua,-1) && number_field(lua,-1,"x",&options->x) && number_field(lua,-1,"y",&options->y) && number_field(lua,-1,"z",&options->z);
	}
	lua_pop(lua,1);
	field(lua,index,"owner");
	if (!lua_isnil(lua,-1))
	{
		PangeaPresentationHandle owner;
		valid=valid && handle_value(lua,-1,&owner);
		if (valid) { options->hasOwner=true; options->ownerID=(int)owner.id; options->ownerGeneration=owner.generation; }
	}
	lua_pop(lua,1);
	return valid;
}

static int result(lua_State* lua, PangeaPresentationStatus status, PangeaPresentationHandle handle)
{
	static const char* reasons[]={"ok","bad-argument","unsupported","asset-not-loaded","capacity-exceeded","stale-handle"};
	lua_createtable(lua,0,4);
	lua_pushboolean(lua,status==PANGEA_PRESENTATION_OK); lua_setfield(lua,-2,"ok");
	lua_pushstring(lua,reasons[status]); lua_setfield(lua,-2,"reason");
	if (status==PANGEA_PRESENTATION_OK && handle.id)
	{
		lua_createtable(lua,0,2);
		lua_pushinteger(lua,handle.id); lua_setfield(lua,-2,"id");
		lua_pushinteger(lua,handle.generation); lua_setfield(lua,-2,"generation");
		lua_setfield(lua,-2,"handle");
	}
	return 1;
}

static int message(lua_State* lua)
{
	char text[PANGEA_PRESENTATION_TEXT]; PangeaPresentationOptions options; PangeaPresentationHandle handle={0};
	if (!string_value(lua,1,text,sizeof(text),false) || !options_value(lua,2,&options)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=PangeaPresentation_Message(text,&options,&handle);
	return result(lua,status,handle);
}

static int objective(lua_State* lua)
{
	char id[PANGEA_PRESENTATION_ID], text[PANGEA_PRESENTATION_TEXT];
	PangeaPresentationOptions options; PangeaPresentationHandle handle={0}; int current=0,total=0;
	if (!string_value(lua,1,id,sizeof(id),false) || !options_value(lua,2,&options) || !lua_istable(lua,2) ||
		!string_field(lua,2,"text",text,sizeof(text),false) || !integer_field(lua,2,"current",&current) || !integer_field(lua,2,"total",&total)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=PangeaPresentation_Objective(id,text,current,total,&options,&handle);
	return result(lua,status,handle);
}

static int play(lua_State* lua)
{
	char id[PANGEA_PRESENTATION_ID]; PangeaPresentationOptions options; PangeaPresentationHandle handle={0};
	if (!string_value(lua,1,id,sizeof(id),false) || !options_value(lua,2,&options)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=PangeaPresentation_Play(id,&options,&handle);
	return result(lua,status,handle);
}

static int effect(lua_State* lua, bool attached)
{
	char id[PANGEA_PRESENTATION_ID]; PangeaPresentationOptions options; PangeaPresentationHandle handle={0};
	if (!string_value(lua,1,id,sizeof(id),false) || !options_value(lua,2,&options)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=PangeaPresentation_Effect(id,&options,attached,&handle);
	return result(lua,status,handle);
}

static int burst(lua_State* lua) { return effect(lua,false); }
static int attach(lua_State* lua) { return effect(lua,true); }

static int remove_resource(lua_State* lua)
{
	PangeaPresentationHandle handle={0};
	if (!handle_value(lua,1,&handle)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=handle.id==1000 ? PangeaDialogue_Cancel(handle) : PangeaPresentation_Remove(handle);
	return result(lua,status,(PangeaPresentationHandle){0});
}

static int active(lua_State* lua)
{
	PangeaPresentationHandle handle={0};
	lua_pushboolean(lua,handle_value(lua,1,&handle) && (PangeaPresentation_Active(handle) || PangeaDialogue_Active(handle)));
	return 1;
}

static bool node_value(lua_State* lua, int index, PangeaDialogueNode* node)
{
	if (!lua_istable(lua,index) || !string_field(lua,index,"text",node->text,sizeof(node->text),false) ||
		!string_field(lua,index,"speaker",node->speaker,sizeof(node->speaker),true) || !string_field(lua,index,"voice",node->voice,sizeof(node->voice),true) ||
		!number_field(lua,index,"duration",&node->duration)) return false;
	int next=node->next+1;
	if (!integer_field(lua,index,"next",&next)) return false;
	node->next=next-1;
	field(lua,index,"choices");
	if (lua_isnil(lua,-1)) { lua_pop(lua,1); return true; }
	if (!lua_istable(lua,-1) || lua_rawlen(lua,-1)>PANGEA_PRESENTATION_CHOICES) { lua_pop(lua,1); return false; }
	node->choiceCount=(int)lua_rawlen(lua,-1);
	bool valid=true;
	for (int i=0; i<node->choiceCount && valid; i++)
	{
		lua_rawgeti(lua,-1,i+1); next=0;
		PangeaDialogueChoice* choice=&node->choices[i];
		valid=lua_istable(lua,-1) && string_field(lua,-1,"id",choice->id,sizeof(choice->id),false) &&
			string_field(lua,-1,"text",choice->text,sizeof(choice->text),false) && integer_field(lua,-1,"next",&next);
		choice->next=next-1;
		lua_pop(lua,1);
	}
	lua_pop(lua,1); return valid;
}

static int define_dialogue(lua_State* lua)
{
	if (!lua_istable(lua,2)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,(PangeaPresentationHandle){0});
	PangeaDialogueDefinition* definition=lua_newuserdatauv(lua,sizeof(*definition),0);
	memset(definition,0,sizeof(*definition));
	bool valid=string_value(lua,1,definition->id,sizeof(definition->id),false) && lua_istable(lua,2) && lua_rawlen(lua,2)<=PANGEA_PRESENTATION_NODES;
	if (valid) definition->nodeCount=(int)lua_rawlen(lua,2);
	for (int i=0; i<definition->nodeCount && valid; i++)
	{
		lua_rawgeti(lua,2,i+1);
		definition->nodes[i].duration=4;
		definition->nodes[i].next=i+1<definition->nodeCount ? i+1 : -1;
		valid=node_value(lua,-1,&definition->nodes[i]);
		lua_pop(lua,1);
	}
	PangeaPresentationStatus status=valid ? PangeaDialogue_Define(definition) : PANGEA_PRESENTATION_BAD_ARGUMENT;
	return result(lua,status,(PangeaPresentationHandle){0});
}

static int start_dialogue(lua_State* lua)
{
	char id[PANGEA_PRESENTATION_ID]; PangeaPresentationOptions options; PangeaPresentationHandle handle={0};
	if (!string_value(lua,1,id,sizeof(id),false) || !options_value(lua,2,&options)) return result(lua,PANGEA_PRESENTATION_BAD_ARGUMENT,handle);
	PangeaPresentationStatus status=PangeaDialogue_Start(id,&options,&handle);
	return result(lua,status,handle);
}

static void function(lua_State* lua, const char* name, lua_CFunction callback)
{
	lua_pushcfunction(lua,callback); lua_setfield(lua,-2,name);
}

void PangeaPresentation_InstallLua(lua_State* lua)
{
	lua_newtable(lua); function(lua,"message",message); function(lua,"objective",objective); function(lua,"remove",remove_resource); function(lua,"isActive",active); lua_setfield(lua,-2,"ui");
	lua_newtable(lua); function(lua,"define",define_dialogue); function(lua,"start",start_dialogue); function(lua,"cancel",remove_resource); function(lua,"isActive",active); lua_setfield(lua,-2,"dialogue");
	lua_newtable(lua); function(lua,"play",play); function(lua,"stop",remove_resource); function(lua,"isActive",active); lua_setfield(lua,-2,"audio");
	lua_newtable(lua);
	function(lua,"burst",burst); function(lua,"attach",attach); function(lua,"stop",remove_resource); lua_setfield(lua,-2,"effects");
}

void PangeaPresentation_PushCapabilities(lua_State* lua)
{
	const PangeaPresentationNative* native=PangeaPresentation_GetNative();
	lua_newtable(lua);
	lua_pushboolean(lua,native->text && (!native->textAvailable || native->textAvailable())); lua_setfield(lua,-2,"text");
	lua_pushboolean(lua,native->choices); lua_setfield(lua,-2,"choices");
	lua_pushboolean(lua,native->positionalAudio); lua_setfield(lua,-2,"positionalAudio");
	lua_pushboolean(lua,native->attachedEffects); lua_setfield(lua,-2,"attachedEffects");
	lua_pushinteger(lua,native->players>0 ? native->players : 1); lua_setfield(lua,-2,"players");
	lua_pushboolean(lua,false); lua_setfield(lua,-2,"modalDialogue");
	lua_pushboolean(lua,false); lua_setfield(lua,-2,"loopingAudio");
	lua_createtable(lua,native->soundCount,0);
	for (int i=0; i<native->soundCount; i++) { lua_pushstring(lua,native->sounds[i]); lua_rawseti(lua,-2,i+1); }
	lua_setfield(lua,-2,"sounds");
	lua_createtable(lua,native->effectCount,0);
	for (int i=0; i<native->effectCount; i++) { lua_pushstring(lua,native->effects[i]); lua_rawseti(lua,-2,i+1); }
	lua_setfield(lua,-2,"effects");
	lua_setfield(lua,-2,"presentation");
}
