// Real production trigger consumer and adapter, executed by the real Lua runtime.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../src/Scripting/ScriptBindings.c"
#include "../src/Items/Triggers.c"

PrefsType gGamePrefs;
Boolean gPlayerGotKilledFlag;
static int deathAnimations;
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed) {(void)skeleton;(void)animation;(void)speed;deathAnimations++;}
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) {(void)skeleton;(void)animation;deathAnimations++;}
static int sounds;
short PlayEffect(int effect) { (void)effect; sounds++; return 0; }

void KnockPlayerBugOnButt(TQ3Vector3D* delta,Boolean ball,Boolean current){(void)delta;(void)ball;(void)current;assert(!"untested knockback branch");}
short PlayEffect3D(int effect,TQ3Point3D* where){(void)effect;(void)where;sounds++;return 0;}
ObjNode* InitPlayer_Bug(ObjNode* old,TQ3Point3D* where,float y,int anim){(void)old;(void)where;(void)y;(void)anim;assert(!"untested ball form branch");return NULL;}

int32_t NewParticleGroup(Byte t,uint8_t flags,float gravity,float magnetism,float scale,float decay,float fade,Byte texture){(void)t;(void)flags;(void)gravity;(void)magnetism;(void)scale;(void)decay;(void)fade;(void)texture;return -1;}
bool AddParticleToGroup(int32_t group,TQ3Point3D* point,TQ3Vector3D* delta,float scale,float alpha){(void)group;(void)point;(void)delta;(void)scale;(void)alpha;return false;}
float RandomFloat(void){return .5f;}
static int removed;
void DeleteObject(ObjNode* o){o->CType=INVALID_NODE_FLAG;removed++;}

TQ3Point3D gCoord;TQ3Vector3D gDelta;float gFramesPerSecondFrac;
void DoFatalAlert(const char* message,...){(void)message;assert(!"native fatal error");}
void GetObjectInfo(ObjNode* o){(void)o;assert(!"untested vanish motion");}
void UpdateObject(ObjNode* o){(void)o;assert(!"untested vanish motion");}
void MakeObjectTransparent(ObjNode* o,float alpha){(void)o;(void)alpha;assert(!"untested vanish motion");}

void UpdateObjectTransforms(ObjNode* o){(void)o;}
void StopAChannel(short* c){(void)c;}
float GetTerrainHeightAtCoord(float x,float z,long layer){(void)x;(void)z;(void)layer;return 10;}
Boolean gTorchPlayer;ObjNode* gCurrentEatingFish;

static char* fixture_text(const char* path, long* outSize)
{
    if (strcmp(path, "Data/Scripts/dist/native-gameplay.lua")) return NULL;
    const char* text = "local d,a,dead,c,p,r,reset=0,0,0,0,0,0,0; return { onDamage=function(ctx) d=d+1; assert(math.abs(ctx.damage-0.2)<0.0001); if d==1 then return {applyDamage=false} end; return {damage=d==2 and 0.1 or 2} end, onDamageApplied=function(ctx) a=a+1; assert(math.abs(ctx.damage-(a==1 and 0.1 or 2))<0.0001) end, onDeath=function() dead=dead+1 end, onPickupCollected=function(ctx) p=p+1; assert(ctx.pickupId=='bugdom.nut'); return {consumePickup=p>1} end, onCheckpointReached=function(ctx) c=c+1; assert(ctx.eventValue==3) end,onPlayerRespawn=function() r=r+1 end,onObjectFrame=function(ctx) if ctx.event=='checkpointReset' then reset=reset+1 end end,onFrame=function() assert(d==3 and a==2 and dead==1 and c==1 and p==2 and r==1 and reset==2, tostring(reset)..':'..tostring(r)) end }";
    *outSize = (long)strlen(text);
    return strdup(text);
}
int main(void)
{
    PangeaScriptGameInfo info = {.gameId="Bugdom",.gameName="Bugdom",.readTextFile=fixture_text};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native-gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={0}; SkeletonObjDataType skeleton={0}; player.Skeleton=&skeleton;gMyHealth=.7f;gPlayerMode=PLAYER_MODE_BUG;
    PangeaScriptObjectHandle handle={0};
    static const PangeaScriptObjectOps ops={.getPosition=BugdomScript_GetObjectPosition};
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&player,.ops=&ops,.objectType="player"},&handle)==PANGEA_SCRIPT_OK);
    player.ScriptObjectID=handle.id; player.ScriptObjectGeneration=handle.generation; gPlayerObj=&player;
    PlayerGotHurt(NULL,.2f,true,false,false,0);
    assert(fabsf(gMyHealth-.7f)<.0001f && player.InvincibleTimer==0);
    PlayerGotHurt(NULL,.2f,true,false,false,0);
    assert(fabsf(gMyHealth-.6f)<.0001f && !gPlayerGotKilledFlag);
    player.InvincibleTimer=0;
    PlayerGotHurt(NULL,.2f,true,false,false,0);
    assert(gPlayerGotKilledFlag && deathAnimations==1);
    KillPlayer(true); // Native duplicate-death guard must suppress duplicate Lua event.
    assert(deathAnimations==1);
    ObjNode checkpoint={.Coord={123,0,456}}, holder={0};checkpoint.ChainHead=&holder;holder.ChainNode=&checkpoint;
    checkpoint.SpecialL[0]=3;checkpoint.SpecialF[3]=.75f;gBestCheckPoint=0;
    assert(!DoTrig_Checkpoint(&checkpoint,&player,0));
    assert(gBestCheckPoint==3 && gMostRecentCheckPointCoord.x==123 && gMostRecentCheckPointCoord.z==456);
    assert(gCheckPointRot==.75f && holder.ChainNode==NULL && removed==1);
    ObjNode powerup={0};PangeaScriptObjectHandle pickupHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&powerup,.ops=&ops,.objectType="pickup"},&pickupHandle)==PANGEA_SCRIPT_OK);
    powerup.ScriptObjectID=pickupHandle.id;powerup.ScriptObjectGeneration=pickupHandle.generation;
    powerup.NutContents=NUT_CONTENTS_HEALTH;powerup.CType=CTYPE_TRIGGER;gMyHealth=.25f;
    assert(!DoTrig_Powerup(&powerup,&player,0));
    assert(gMyHealth==.25f && powerup.CType==CTYPE_TRIGGER && !powerup.MoveCall);
    assert(!DoTrig_Powerup(&powerup,&player,0));
    assert(gMyHealth==.75f && powerup.CType==0 && powerup.MoveCall);
    ResetPlayer();
    assert(!gPlayerGotKilledFlag && gMyHealth==1 && player.Coord.x==123 && player.Coord.z==456);
    assert(player.InvincibleTimer>0 && player.CType==CTYPE_PLAYER);
    PangeaScriptStatus finalStatus=PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){0});
    if(finalStatus!=PANGEA_SCRIPT_OK) fprintf(stderr,"%s\n",PangeaScript_GetLastError());
    assert(finalStatus==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetErrorCount()==0);
    PangeaScript_Shutdown();
    return 0;
}
