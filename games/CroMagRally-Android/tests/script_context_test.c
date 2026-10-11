#include "game.h"
#include "ScriptBindings.h"
#include <assert.h>
#include <math.h>

int gGameMode;
int gTrackNum;
static PangeaScriptWeaponHitContext weaponContext;
static bool targetExists = true;
static bool removeTarget;
Boolean gNetGameInProgress;
static PangeaScriptFrameContext frame;
static PangeaScriptLevelContext level;
PangeaScriptStatus PangeaScript_CallLevelHook(PangeaScriptHook hook, const PangeaScriptLevelContext* ctx)
{ (void)hook;level=*ctx;return PANGEA_SCRIPT_OK; }
PangeaScriptStatus PangeaScript_CallFrameHook(const PangeaScriptFrameContext* ctx)
{ frame=*ctx;return PANGEA_SCRIPT_OK; }
void PangeaScript_ExpireTriggerContacts(const PangeaScriptFrameContext* ctx) { (void)ctx; }
PangeaScriptStatus PangeaScript_ApplyDeferredActions(const PangeaScriptFrameContext* ctx)
{ (void)ctx;return PANGEA_SCRIPT_OK; }
const char* PangeaScript_GetLastError(void) { return "test"; }
bool PangeaScript_ObjectExists(PangeaScriptObjectHandle handle) { (void)handle;return targetExists; }
PangeaScriptStatus PangeaScript_CallWeaponHitHook(const PangeaScriptWeaponHitContext* ctx, PangeaScriptWeaponHitResult* result)
{
    weaponContext=*ctx;
    if (removeTarget) targetExists=false;
    *result=(PangeaScriptWeaponHitResult){.damage=.25f,.hasApplyDamage=true,.applyDamage=false,.destroyTarget=true};
    return PANGEA_SCRIPT_OK;
}
int main(void)
{
    gGameMode=GAME_MODE_MULTIPLAYERRACE;gNetGameInProgress=false;
    CroMagScript_OnRaceStart(3);assert(!level.networked);
    CroMagScript_OnRaceFrame(3,1,.01f,100);
    CroMagScript_OnRaceFrame(3,2,.03f,100);
    assert(!frame.networked && frame.levelNum==3);
    assert(fabsf(frame.levelTimeSeconds-.04f)<.00001f);
    CroMagScript_OnRaceStart(4);
    CroMagScript_OnRaceFrame(4,3,.02f,100);
    assert(fabsf(frame.levelTimeSeconds-.02f)<.00001f);
    gTrackNum=7;
    ObjNode weapon={.ScriptObjectID=11,.ScriptObjectGeneration=2};
    ObjNode target={.ScriptObjectID=12,.ScriptObjectGeneration=3,.Coord={10,20,30}};
    float damage;Boolean destroy;
    assert(!CroMagScript_OnWeaponHit(&weapon,&target,1,POW_TYPE_BONE,.5f,&damage,&destroy));
    assert(damage==.25f && destroy);
    assert(weaponContext.levelNum==7 && weaponContext.playerNum==1);
    assert(weaponContext.weapon.id==11 && weaponContext.target.id==12);
    assert(weaponContext.weaponType==POW_TYPE_BONE && weaponContext.position.y==20);
    removeTarget=true;
    assert(!CroMagScript_OnWeaponHit(&weapon,&target,1,POW_TYPE_BONE,.5f,&damage,&destroy));
    return 0;
}
