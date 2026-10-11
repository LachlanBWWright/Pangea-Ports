#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../Source/Scripting/ScriptBindings.c"

// Only rendering/audio dependencies are stubbed; unexpected gameplay branches fail.
void MorphToSkeletonAnim(SkeletonObjDataType* s,long a,float v){(void)s;(void)a;(void)v;}
void SetSkeletonAnim(SkeletonObjDataType* s,long a){(void)s;(void)a;}
void KillPlayerOnBalsaPlane(ObjNode* o){(void)o;assert(!"untested balsa death branch");}
void KillPlayerInTunnel(ObjNode* o){(void)o;assert(!"untested tunnel death branch");}
void MakeSparkExplosion(float x,float y,float z,float f,float s,short t,short q,float fade){(void)x;(void)y;(void)z;(void)f;(void)s;(void)t;(void)q;(void)fade;assert(!"untested explosion branch");}
void ExplodeGeometry(ObjNode* o,float f,Byte m,long d,float s){(void)o;(void)f;(void)m;(void)d;(void)s;assert(!"untested explosion branch");}
void DeleteObject(ObjNode* o){(void)o;assert(!"unexpected wing deletion");}
void StopAChannel(short* c){(void)c;}

uint32_t gScore;OGLPoint2D gBestCheckpointCoord;float gBestCheckpointAim,gFramesPerSecondFrac;OGLPoint3D gCoord;
Boolean gIgnoreBottleKeySnail;int gLevelNum;
void ReleaseHeldObject(ObjNode* p,ObjNode* held){(void)p;(void)held;assert(!"untested ramming branch");}
int LevelMetadataCaseFor(const char* key,int fallback){(void)key;return fallback;}
short PlayEffect(int e){(void)e;return -1;}
short PlayEffect3D(int e,OGLPoint3D* p){(void)e;(void)p;return -1;}
void PlayRumbleEffect(int e){(void)e;}
void GetObjectInfo(ObjNode* o){(void)o;assert(!"vanish movement not exercised");}
void UpdateObject(ObjNode* o){(void)o;assert(!"vanish movement not exercised");}

static int collisionUpdates;
void CalcObjectBoxFromNode(ObjNode* o){assert(o->CollisionBoxes && o->NumCollisionBoxes==1);collisionUpdates++;}
void UpdateObjectTransforms(ObjNode* o){(void)o;}

Boolean gGameOver,gResetRideBall,gEnableSnakes;ObjNode* gKillerDragonFly;
void DoPlayerTerrainUpdate(float x,float z){(void)x;(void)z;}
float FindHighestCollisionAtXZ(float x,float z,uint32_t type){(void)x;(void)z;(void)type;return 10;}
void ResetPlayerOnBalsaPlane(ObjNode* o){(void)o;assert(!"untested balsa respawn branch");}
void ResetRisingWater(void){assert(!"untested garbage respawn branch");}
void InitCamera_Terrain(void){}
ObjNode* MakeFadeEvent(Boolean in,float speed){(void)in;(void)speed;return NULL;}
float RandomFloat(void){return .5f;}

static char* fixture_text(const char* path, long* outSize)
{
    if (strcmp(path,"Data/Scripts/dist/native-gameplay.lua")) return NULL;
    const char* text="local d,a,dead,p,c,r,reset=0,0,0,0,0,0,0; return {onDamage=function(ctx) d=d+1; if d==1 then return {applyDamage=false} end; return {damage=d==2 and 0.25 or 2} end,onDamageApplied=function(ctx) a=a+1; assert(math.abs(ctx.damage-(a==1 and 0.25 or 2))<0.0001) end,onDeath=function(ctx) dead=dead+1; assert(ctx.eventValue==0) end,onPickupCollected=function(ctx) p=p+1; assert(ctx.pickupId=='bugdom2.powerup'); return {consumePickup=p>1} end,onCheckpointReached=function(ctx) c=c+1; assert(ctx.eventValue==3) end,onPlayerRespawn=function() r=r+1 end,onObjectFrame=function(ctx) if ctx.event=='checkpointReset' then reset=reset+1 end end,onFrame=function() assert(d==3 and a==2 and dead==1 and p==2 and c==1 and r==1 and reset==3) end}";
    *outSize=(long)strlen(text); return strdup(text);
}
int main(void)
{
    PangeaScriptGameInfo info={.gameId="Bugdom2",.gameName="Bugdom 2",.readTextFile=fixture_text};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native-gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={0}; SkeletonObjDataType skeleton={0}; player.Skeleton=&skeleton;
    gPlayerInfo.objNode=&player; gPlayerInfo.health=1;
    static const PangeaScriptObjectOps ops={.getPosition=Bugdom2Script_GetObjectPosition};
    PangeaScriptObjectHandle handle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&player,.ops=&ops,.objectType="player"},&handle)==PANGEA_SCRIPT_OK);
    player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
    assert(!PlayerLoseHealth(.1f,0));assert(gPlayerInfo.health==1);
    assert(!PlayerLoseHealth(.2f,0));assert(gPlayerInfo.health==.75f);
    assert(PlayerLoseHealth(2,0));assert(gPlayerIsDead && gPlayerInfo.health==0);
    KillPlayer(0);
    ObjNode powerup={0}, checkpoint={.Coord={123,0,456}};
    PangeaScriptObjectHandle powerupHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&powerup,.ops=&ops,.objectType="pickup"},&powerupHandle)==PANGEA_SCRIPT_OK);
    powerup.ScriptObjectID=powerupHandle.id;powerup.ScriptObjectGeneration=powerupHandle.generation;
    powerup.Special[0]=POW_KIND_HEALTH;powerup.CType=CTYPE_TRIGGER;gPlayerInfo.health=.5f;
    assert(!DoTrig_Powerup(&powerup,&player,0));
    assert(gPlayerInfo.health==.5f && powerup.CType==CTYPE_TRIGGER && !powerup.MoveCall);
    assert(DoTrig_Powerup(&powerup,&player,0));
    assert(fabsf(gPlayerInfo.health-.65f)<.0001f && powerup.CType==0 && powerup.MoveCall);
    checkpoint.Special[0]=3;player.Rot.y=.75f;SetCheckpoint(&checkpoint,&player);
    assert(gBestCheckpointCoord.x==123 && gBestCheckpointCoord.y==456 && gBestCheckpointAim==.75f);
    ObjNode decorative={0};PangeaScriptObjectHandle decorativeHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&decorative,.ops=&ops,.objectType="decorative"},&decorativeHandle)==PANGEA_SCRIPT_OK);
    decorative.ScriptObjectID=decorativeHandle.id;decorative.ScriptObjectGeneration=decorativeHandle.generation;
    Bugdom2Script_ApplyObjectScripting(&decorative);
    assert(collisionUpdates==0); // Native decorative objects do not own a collision box.
    gPlayerInfo.lives=3;gLevelNum=LEVEL_NUM_PARK;ResetPlayerAtBestCheckpoint();
    assert(gPlayerInfo.lives==2 && !gPlayerIsDead && gPlayerInfo.health==1);
    assert(player.Coord.x==123 && player.Coord.z==456 && player.CType==CTYPE_PLAYER && player.Rot.y==.75f);
    assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){0})==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetErrorCount()==0);PangeaScript_Shutdown();return 0;
}
