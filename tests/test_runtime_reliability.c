#include "../shared/script/pangea_script_backend.h"
#include "../shared/script/pangea_presentation.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gNativeCalls;

static PangeaScriptStatus save_persistent(const char* key, const unsigned char* data, int size)
{
	(void)key; (void)data; (void)size;
	gNativeCalls++;
	return PANGEA_SCRIPT_OK;
}

static PangeaPresentationStatus play(const char* id, const PangeaPresentationOptions* options, int* voice)
{
	(void)id; (void)options;
	*voice = 1;
	gNativeCalls++;
	return PANGEA_PRESENTATION_OK;
}

static void call_frame(PangeaScriptBackend* backend, unsigned int frameNum, float time)
{
	char error[1024] = {0};
	PangeaScriptFrameContext frame = {.frameNum = frameNum, .deltaSeconds = .1f, .levelTimeSeconds = time};
	PangeaScriptStatus status = PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error));
	if (status != PANGEA_SCRIPT_OK) fprintf(stderr, "frame failed: %s\n", error);
	assert(status == PANGEA_SCRIPT_OK);
}

static void test_reload_preserves_working_state(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	const char* original =
		"local count=0 local fired=false "
		"pangea.time.after(.5,function() fired=true end) "
		"return {onFrame=function(ctx) count=count+1 assert(count==ctx.frameNum) "
		"if ctx.levelTimeSeconds>=.5 then assert(fired) end end}";
	assert(PangeaScriptBackend_Load(backend, original, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	PangeaPresentationOptions options = PangeaPresentation_DefaultOptions();
	PangeaPresentationHandle objective;
	assert(PangeaPresentation_Objective("keep", "Keep existing objective", 0, 1, &options, &objective) == PANGEA_PRESENTATION_OK);
	call_frame(backend, 1, .1f);
	assert(PangeaScriptBackend_Reload(backend, "return {", error, sizeof(error)) == PANGEA_SCRIPT_PARSE_ERROR);
	assert(PangeaPresentation_Active(objective));
	call_frame(backend, 2, .2f);
	assert(PangeaScriptBackend_Reload(backend, "error('replacement failed')", error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "replacement failed"));
	call_frame(backend, 3, .3f);
	assert(PangeaScriptBackend_Reload(backend, "return false", error, sizeof(error)) == PANGEA_SCRIPT_PARSE_ERROR);
	call_frame(backend, 4, .4f);
	assert(PangeaScriptBackend_Reload(backend, "while true do end", error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	call_frame(backend, 5, .5f);
	assert(PangeaScriptBackend_Reload(backend, "local exhausted=string.rep('x',32*1024*1024) return {}", error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	assert(PangeaPresentation_Active(objective));
	call_frame(backend, 6, .6f);
}

static void test_staging_blocks_native_mutations(PangeaScriptBackend* backend)
{
	const char* sources[] = {
		"pangea.ui.message('must not appear') return {}",
		"pangea.audio.play('test') return {}",
		"pangea.dialogue.define('test',{}) return {}",
		"pangea.effects.burst('test') return {}",
		"pangea.spawn.native('test',{x=0,y=0,z=0}) return {}",
		"pangea.object.setHealth({id=1,generation=1},1) return {}",
		"pangea.player.setHealth(0,1) return {}",
		"pangea.level.setRulesControlled(true) return {}",
		"pangea.persistence.set('must-not-write',1,true) return {}",
		"local audio=require('pangea').audio pangea.task.start(function() audio.play('test') end) error('discard')",
		"local finalizer=setmetatable({},{__gc=function() pangea.audio.play('test') end}) error('discard')",
	};
	char error[1024] = {0};
	for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
	{
		assert(PangeaScriptBackend_Reload(backend, sources[i], error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
		assert(gNativeCalls == 0);
	}
	call_frame(backend, 7, .7f);
}

static void test_success_commits_candidate(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	PangeaPresentationOptions options = PangeaPresentation_DefaultOptions();
	PangeaPresentationHandle objective;
	assert(PangeaPresentation_Objective("retire", "Retire on success", 0, 1, &options, &objective) == PANGEA_PRESENTATION_OK);
	const char* replacement =
		"local fired=false local events=0 "
		"pangea.events.on('committed',function() events=events+1 end) "
		"pangea.time.after(.2,function() fired=true end) "
		"return {onFrame=function() assert(fired) pangea.events.emit('committed') assert(events==1) "
		"assert(pangea.audio.play('test').ok) end}";
	assert(PangeaScriptBackend_Reload(backend, replacement, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	assert(!PangeaPresentation_Active(objective));
	call_frame(backend, 8, .2f);
	assert(gNativeCalls == 1);
}

static void test_context_allocation_is_protected(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	assert(PangeaScriptBackend_Load(backend, "return {onFrame=function(ctx) assert(ctx.frameNum==42) end}", error, sizeof(error)) == PANGEA_SCRIPT_OK);
	size_t length = 32 * 1024 * 1024;
	char* name = malloc(length + 1);
	assert(name);
	memset(name, 'x', length);
	name[length] = 0;
	PangeaScriptFrameContext frame = {.frameNum = 42, .levelName = name};
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_BUDGET_EXCEEDED);
	free(name);
	frame.levelName = "recovered";
	assert(PangeaScriptBackend_CallFrameHook(backend, &frame, error, sizeof(error)) == PANGEA_SCRIPT_OK);
}

static void test_result_metamethod_is_protected(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	const char* sources[] = {
		"return {onDamage=function() return setmetatable({},{__index=function() error('hostile result') end}) end}",
		"return {onDamage=function() return setmetatable({},{__index=function() while true do end end}) end}",
		"return {onDamage=function() while true do end end}",
		"return {onDamage=function() return {damage=1e300} end}",
	};
	PangeaScriptDamageContext damage = {.damage = 1};
	for (int i = 0; i < 4; i++)
	{
		assert(PangeaScriptBackend_Load(backend, sources[i], error, sizeof(error)) == PANGEA_SCRIPT_OK);
		PangeaScriptDamageResult result = {0};
		PangeaScriptStatus expected = i == 1 || i == 2 ? PANGEA_SCRIPT_BUDGET_EXCEEDED : PANGEA_SCRIPT_RUNTIME_ERROR;
		assert(PangeaScriptBackend_CallDamageHook(backend, &damage, &result, error, sizeof(error)) == expected);
		assert(PangeaScriptBackend_CallDamageHook(backend, &damage, &result, NULL, 0) == expected);
		call_frame(backend, 1, .1f);
	}
}

static void test_finalizers_cannot_escape_budgets(PangeaScriptBackend* backend)
{
	char error[1024] = {0};
	const char* attempt =
		"local mt={} local object=setmetatable({},mt) "
		"mt.__gc=function() while true do end end "
		"local ok,message=pcall(setmetatable,object,mt) "
		"assert(not ok and string.find(message,'__gc',1,true)) "
		"object=nil collectgarbage('collect') "
		"local native=pangea.audio.play "
		"local ok2=pcall(setmetatable,{},{__gc=function() native('test') end}) "
		"assert(not ok2) "
		"local pendingMeta={} local pending=setmetatable({},pendingMeta) "
		"pendingMeta.__gc=function() while true do end end "
		"return {retained=pending}";
	assert(PangeaScriptBackend_Load(backend, attempt, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	const char* infinite = "local object=setmetatable({},{__gc=function() while true do end end}) return {retained=object}";
	assert(PangeaScriptBackend_Reload(backend, infinite, error, sizeof(error)) == PANGEA_SCRIPT_RUNTIME_ERROR);
	assert(strstr(error, "__gc"));
	call_frame(backend, 1, .1f);
	assert(PangeaScriptBackend_Reload(backend, attempt, error, sizeof(error)) == PANGEA_SCRIPT_OK);
	call_frame(backend, 2, .2f);
	assert(gNativeCalls == 1);
}

int main(void)
{
	const char* sounds[] = {"test"};
	PangeaPresentationNative native = {.text = true, .play = play, .sounds = sounds, .soundCount = 1};
	PangeaPresentation_Configure(&native);
	PangeaScriptGameInfo game = {.gameId = "OttoMatic-Android", .gameName = "Reliability", .savePersistent = save_persistent};
	PangeaScriptBackend* backend = PangeaScriptBackend_Create(&game);
	assert(backend);
	test_reload_preserves_working_state(backend);
	test_staging_blocks_native_mutations(backend);
	test_success_commits_candidate(backend);
	test_context_allocation_is_protected(backend);
	test_result_metamethod_is_protected(backend);
	test_finalizers_cannot_escape_budgets(backend);
	PangeaScriptBackend_Destroy(backend);
	PangeaPresentation_Reset();
	puts("Runtime reliability tests passed");
	return 0;
}
