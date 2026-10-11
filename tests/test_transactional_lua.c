#include "pangea_script.h"
#include "pangea_script_config.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* configText;
static const char* replacementText;
static int nativeWrites;
static int persistentWrites;
static PangeaScriptVector3 position;

static const char stableModule[] =
    "local setting = pangea.level.setting('value')\n"
    "assert(setting == 7, 'candidate must see its own configuration')\n"
    "local frames = 0\n"
    "return { onFrame = function(ctx)\n"
    "  assert(setting == 7 and pangea.level.setting('value') == 7)\n"
    "  if ctx.mode == 'check-failure' then\n"
    "    local failure = assert(pangea.api.diagnostics().lastError)\n"
    "    assert(failure.source == 'Data/Scripts/replacement.lua')\n"
    "    assert(failure.gameId == 'transactional-test')\n"
    "    assert(type(failure.traceback) == 'string' and #failure.traceback > 0)\n"
    "  end\n"
    "  frames = frames + 1\n"
    "  local object = assert(pangea.object.findByTag('target')[1])\n"
    "  assert(pangea.object.setPosition(object, {x=frames,y=0,z=0}))\n"
    "end }";

static char* read_file(const char* path, long* size)
{
    const char* text = NULL;
    if (strcmp(path, "Data/Scripts/config/transactional.json") == 0) text = configText;
    else if (strcmp(path, "Data/Scripts/stable.lua") == 0) text = stableModule;
    else if (strcmp(path, "Data/Scripts/replacement.lua") == 0) text = replacementText;
    if (!text) return NULL;
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    assert(copy);
    memcpy(copy, text, length + 1);
    *size = (long)length;
    return copy;
}

static bool get_position(void* nativeObject, PangeaScriptVector3* result)
{
    *result = *(PangeaScriptVector3*)nativeObject;
    return true;
}

static bool set_position(void* nativeObject, const PangeaScriptVector3* value)
{
    *(PangeaScriptVector3*)nativeObject = *value;
    nativeWrites++;
    return true;
}

static PangeaScriptStatus load_persistent(const char* key, unsigned char* data, int capacity, int* size)
{
    (void)key; (void)data; (void)capacity;
    *size = 0;
    return PANGEA_SCRIPT_FILE_NOT_FOUND;
}

static PangeaScriptStatus save_persistent(const char* key, const unsigned char* data, int size)
{
    (void)key; (void)data; (void)size;
    persistentWrites++;
    return PANGEA_SCRIPT_OK;
}

static void assert_unchanged(PangeaScriptObjectHandle object, uint32_t fingerprint, int writes)
{
    assert(PangeaScript_HasRunnableModule());
    assert(PangeaScript_ObjectExists(object));
    assert(PangeaScript_GetRegisteredObjectCount() == 1);
    assert(PangeaScript_GetRuntimeFingerprint() == fingerprint);
    assert(strcmp(PangeaScript_GetStatusActiveScriptPath(), "Data/Scripts/stable.lua") == 0);
    int value = 0;
    assert(PangeaScript_GetLevelIntSetting("value", &value) && value == 7);
    assert(nativeWrites == writes && persistentWrites == 0);
}

int main(void)
{
    PangeaScriptGameInfo game = {
        .gameId = "transactional-test", .gameName = "Transactional Lua tests",
        .readTextFile = read_file, .loadPersistent = load_persistent, .savePersistent = save_persistent,
    };
    assert(PangeaScript_Init(&game) == PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetConfigPath("Data/Scripts/config/transactional.json") == PANGEA_SCRIPT_OK);
    configText = "{\"version\":1,\"levels\":{\"0\":{\"script\":\"Data/Scripts/stable.lua\",\"levelSettings\":{\"value\":7}}}}";
    assert(PangeaScript_LoadLevelConfig(0) == PANGEA_SCRIPT_OK);
    const char* tags[] = {"target"};
    const PangeaScriptObjectOps ops = {.getPosition = get_position, .setPosition = set_position};
    PangeaScriptObjectRegistration registration = {
        .nativeObject = &position, .ops = &ops, .objectType = "target", .tags = tags, .tagCount = 1,
        .capabilityLevel = PANGEA_SCRIPT_CAPABILITY_FULL,
    };
    PangeaScriptObjectHandle object;
    assert(PangeaScript_RegisterObject(&registration, &object) == PANGEA_SCRIPT_OK);
    PangeaScriptFrameContext frame = {.levelNum = 0, .frameNum = 1, .levelTimeSeconds = 100};
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    assert(position.x == 1);
    uint32_t fingerprint = PangeaScript_GetRuntimeFingerprint();

    const char* failures[] = {
        "return { onFrame = function( }", "error('replacement initialization failed')",
        "pangea.persistence.set('forbidden', 1, 'changed'); return {}",
        "local object=pangea.object.findByTag('target')[1]; pangea.object.setPosition(object,{x=999,y=0,z=0}); return {}",
        "pangea.task.start(function() local object=pangea.object.findByTag('target')[1]; pangea.object.setPosition(object,{x=999,y=0,z=0}) end); return {}",
    };
    for (unsigned int i = 0; i < sizeof failures / sizeof failures[0]; i++)
    {
        replacementText = failures[i];
        int writes = nativeWrites;
        assert(PangeaScript_SetStartupScript("Data/Scripts/replacement.lua") != PANGEA_SCRIPT_OK);
        assert_unchanged(object, fingerprint, writes);
        frame.frameNum++;
        frame.mode = "check-failure";
        assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
        assert(position.x == (float)frame.frameNum);
    }

    /* Config failures preserve both native handles and the old module's private state. */
    configText = "{\"version\":1,\"levels\":{\"1\":{\"script\":\"Data/Scripts/replacement.lua\",\"levelSettings\":{\"value\":99}}}}";
    replacementText = "assert(pangea.level.setting('value') == 99); error('reject new config')";
    int writes = nativeWrites;
    assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_RUNTIME_ERROR);
    assert(strstr(PangeaScript_GetLastError(), "reject new config"));
    assert_unchanged(object, fingerprint, writes);
    frame.frameNum++;
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    assert(position.x == (float)frame.frameNum);

    /* Capacity overflow must reject the entire config, preserving the live world. */
    char boundaryConfig[16384];
    for (int assets = 0; assets < 2; assets++)
    {
        for (int count = 64; count <= 65; count++)
        {
            int used = snprintf(boundaryConfig, sizeof boundaryConfig,
                "{\"version\":1,\"levels\":{\"1\":{\"levelSettings\":{");
            if (assets) used += snprintf(boundaryConfig + used, sizeof boundaryConfig - used, "\"assetDependencies\":[");
            for (int i = 0; i < count; i++)
            {
                used += snprintf(boundaryConfig + used, sizeof boundaryConfig - used,
                    assets ? "%s{\"kind\":\"model\",\"id\":\"asset%d\"}" : "%s\"value%d\":7", i ? "," : "", i);
            }
            snprintf(boundaryConfig + used, sizeof boundaryConfig - used, assets ? "]}}}}" : "}}}}");
            PangeaConfig parsed;
            char error[256];
            PangeaScriptStatus status = PangeaScript_ParseConfig(boundaryConfig, 1, &parsed, error, sizeof error);
            if (count == 64)
            {
                assert(status == PANGEA_SCRIPT_OK);
                assert((assets ? parsed.level.assetDependencyCount : parsed.level.levelSettingCount) == 64);
            }
            else
            {
                assert(status == PANGEA_SCRIPT_CONFIG_ERROR && strstr(error, "limit of 64"));
                configText = boundaryConfig;
                writes = nativeWrites;
                assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_CONFIG_ERROR);
                assert_unchanged(object, fingerprint, writes);
                frame.frameNum++;
                frame.mode = NULL;
                assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
                assert(position.x == (float)frame.frameNum);
            }
        }
    }
    /* Asset identifier limits are byte limits and must never silently truncate. */
    for (int field = 0; field < 2; field++)
    {
        int capacity = field ? 96 : 32;
        char text[97];
        memset(text, 'x', sizeof text);
        for (int length = capacity - 1; length <= capacity; length++)
        {
            text[length] = '\0';
            snprintf(boundaryConfig, sizeof boundaryConfig,
                "{\"version\":1,\"levels\":{\"1\":{\"levelSettings\":{\"assetDependencies\":[{\"kind\":\"%s\",\"id\":\"%s\"}]}}}}",
                field ? "model" : text, field ? text : "asset");
            PangeaConfig parsed;
            char error[256];
            PangeaScriptStatus status = PangeaScript_ParseConfig(boundaryConfig, 1, &parsed, error, sizeof error);
            if (length < capacity)
            {
                assert(status == PANGEA_SCRIPT_OK && parsed.level.assetDependencyCount == 1);
                assert(strlen(field ? parsed.level.assetDependencies[0].id : parsed.level.assetDependencies[0].kind) == (size_t)length);
            }
            else
            {
                assert(status == PANGEA_SCRIPT_CONFIG_ERROR);
                configText = boundaryConfig;
                assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_CONFIG_ERROR);
                assert_unchanged(object, fingerprint, nativeWrites);
            }
            text[length] = 'x';
        }
    }
    /* Missing files and malformed JSON also leave the live module runnable. */
    replacementText = NULL;
    assert(PangeaScript_SetStartupScript("Data/Scripts/replacement.lua") == PANGEA_SCRIPT_FILE_NOT_FOUND);
    assert_unchanged(object, fingerprint, nativeWrites);
    configText = NULL;
    assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_FILE_NOT_FOUND);
    assert_unchanged(object, fingerprint, nativeWrites);
    configText = "{\"version\":1,\"levels\":";
    assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_CONFIG_ERROR);
    assert_unchanged(object, fingerprint, nativeWrites);
    configText = "{\"version\":1,\"levels\":{\"1\":{\"script\":\"Data/Scripts/replacement.lua\",\"levelSettings\":{\"value\":99}}}}";

    /* Accepted replacement initialization reads the incoming, not previous, settings. */
    replacementText =
        "local value = pangea.level.setting('value'); assert(value == 99)\n"
        "return {onLevelStart=function(ctx) assert(ctx.levelNum==1 and value==99 and pangea.level.current()==1) end}";
    assert(PangeaScript_LoadLevelConfig(1) == PANGEA_SCRIPT_OK);
    assert(!PangeaScript_ObjectExists(object));
    PangeaScriptLevelContext level = {.levelNum = 1};
    assert(PangeaScript_CallLevelHook(PANGEA_SCRIPT_HOOK_LEVEL_START, &level) == PANGEA_SCRIPT_OK);

    /* Reload timers use the current level clock, including at module initialization. */
    frame = (PangeaScriptFrameContext){.levelNum = 1, .frameNum = 50, .levelTimeSeconds = 100};
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    replacementText =
        "assert(pangea.level.current()==1 and pangea.time.level()==100)\n"
        "local fired=0; pangea.time.after(1,function() fired=fired+1 end)\n"
        "return {onFrame=function(ctx) if ctx.levelTimeSeconds<101 then assert(fired==0) else assert(fired==1) end end}";
    assert(PangeaScript_Reload() == PANGEA_SCRIPT_OK);
    frame.frameNum++; frame.levelTimeSeconds = 100.5f;
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    frame.frameNum++; frame.levelTimeSeconds = 101;
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    frame.frameNum++; frame.levelTimeSeconds = 102;
    assert(PangeaScript_CallFrameHook(&frame) == PANGEA_SCRIPT_OK);
    PangeaScript_Shutdown();
    puts("Real Lua reload rollback, staging isolation, incoming configuration and reload timer clock passed");
    return 0;
}
