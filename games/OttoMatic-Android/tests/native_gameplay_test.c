#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../src/Scripting/ScriptBindings.c"
Boolean UntestedProductionDischargeSuperNova(void);
#define DischargeSuperNova UntestedProductionDischargeSuperNova
#include "../src/Player/Player_Weapons.c"
#undef DischargeSuperNova

// Rendering/audio dependencies are inert; native health/death decisions remain production code.
OGLPoint3D gCoord; OGLVector3D gDelta; ObjNode* gFirstNodePtr;
void UpdateObjectTransforms(ObjNode* o){(void)o;}
void MorphToSkeletonAnim(SkeletonObjDataType* s,long a,float v){(void)s;(void)a;(void)v;}
void SetSkeletonAnim(SkeletonObjDataType* s,long a){(void)s;(void)a;}
void Rumble(float s,uint32_t ms){(void)s;(void)ms;}
short PlayEffect_Parms3D(short e,OGLPoint3D* p,uint32_t r,float v){(void)e;(void)p;(void)r;(void)v;return -1;}
short PlayEffect3D(short e,OGLPoint3D* p){(void)e;(void)p;return -1;}
void BlowUpSaucer(ObjNode* o){(void)o;assert(!"untested saucer death branch");}
Boolean DischargeSuperNova(void){assert(!"untested supernova branch");return false;}
void MakeSparkExplosion(float x,float y,float z,float f,float s,short t,short q){(void)x;(void)y;(void)z;(void)f;(void)s;(void)t;(void)q;assert(!"untested explosion branch");}
void ExplodeGeometry(ObjNode* o,float f,Byte m,long d,float s){(void)o;(void)f;(void)m;(void)d;(void)s;assert(!"untested explosion branch");}
void StartDeathExit(float d){(void)d;assert(!"untested explosion branch");}

OGLPoint2D gBestCheckpointCoord;float gBestCheckpointAim;int gLevelNum;
void DoPlayerTerrainUpdate(float x,float z){(void)x;(void)z;}
Boolean GetLevelMetadataBool(const char* key,Boolean fallback){(void)key;return fallback;}
void InitCamera(void){}
short PlayEffect_Parms(int e,uint32_t l,uint32_t r,unsigned long f){(void)e;(void)l;(void)r;(void)f;return -1;}
static int hitHandlers,deleted;
void DeleteObject(ObjNode* o){o->CType=INVALID_NODE_FLAG;deleted++;}
static Boolean target_hit(ObjNode* weapon,ObjNode* target,OGLPoint3D* where,OGLVector3D* delta){(void)where;(void)delta;target->Health-=weapon->Damage;hitHandlers++;return false;}

uint32_t gScore;
void DeleteEnemy(ObjNode* o){(void)o;assert(!"untested enemy deletion branch");}
float FindHighestCollisionAtXZ(float x,float z,uint32_t c){(void)x;(void)z;(void)c;assert(!"untested terrain respawn branch");return 0;}
void SetPlayerStandAnim(ObjNode* o,float s){(void)o;(void)s;assert(!"untested terrain respawn branch");}

static char* fixture_text(const char* path, long* outSize)
{
    if (strcmp(path,"Data/Scripts/dist/native-gameplay.lua")) return NULL;
    const char* text="local d,a,dead,r,w=0,0,0,0,0; return {onDamage=function(ctx) d=d+1; if d==1 then return {applyDamage=false} end; return {damage=d==2 and 0.25 or 2} end,onDamageApplied=function(ctx) a=a+1; assert(math.abs(ctx.damage-(a==1 and 0.25 or 2))<0.0001) end,onDeath=function(ctx) dead=dead+1; assert(ctx.eventValue==0) end,onPlayerRespawn=function() r=r+1 end,onWeaponHit=function(ctx) w=w+1; if w==1 then return {applyDamage=false} elseif w==2 then return {damage=0.1} else return {applyDamage=false,destroyTarget=true} end end,onFrame=function() assert(d==3 and a==2 and dead==1 and r==1 and w==3) end}";
    *outSize=(long)strlen(text); return strdup(text);
}
int main(void)
{
    PangeaScriptGameInfo info={.gameId="OttoMatic",.gameName="Otto Matic",.readTextFile=fixture_text};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native-gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={0}; SkeletonObjDataType skeleton={0}; player.Skeleton=&skeleton;
    gPlayerInfo.objNode=&player; gPlayerInfo.health=1;
    static const PangeaScriptObjectOps ops={.getPosition=OttoObjectGetPosition};
    PangeaScriptObjectHandle handle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&player,.ops=&ops,.objectType="player"},&handle)==PANGEA_SCRIPT_OK);
    player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
    assert(!PlayerLoseHealth(.1f,0));assert(gPlayerInfo.health==1);
    assert(!PlayerLoseHealth(.2f,0));assert(gPlayerInfo.health==.75f);
    assert(PlayerLoseHealth(2,0));assert(gPlayerIsDead && gPlayerInfo.health==0);
    KillPlayer(0);
    gLevelNum=LEVEL_NUM_SAUCER;gBestCheckpointCoord=(OGLPoint2D){123,456};ResetPlayerAtBestCheckpoint();
    assert(!gPlayerIsDead && gPlayerInfo.health==1 && player.Coord.x==123 && player.Coord.z==456);
    ObjNode weapon={.Damage=.5f}, target={.Health=1};
    target.HitByWeaponHandler[0]=target_hit;
    PangeaScriptObjectHandle weaponHandle,targetHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&weapon,.ops=&ops,.objectType="weapon"},&weaponHandle)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&target,.ops=&ops,.objectType="target"},&targetHandle)==PANGEA_SCRIPT_OK);
    weapon.ScriptObjectID=weaponHandle.id;weapon.ScriptObjectGeneration=weaponHandle.generation;
    target.ScriptObjectID=targetHandle.id;target.ScriptObjectGeneration=targetHandle.generation;
    assert(!OttoScript_ApplyWeaponHit(&weapon,&target) && target.Health==1 && hitHandlers==0);
    assert(!OttoScript_ApplyWeaponHit(&weapon,&target) && fabsf(target.Health-.9f)<.0001f && hitHandlers==1 && weapon.Damage==.5f);
    assert(OttoScript_ApplyWeaponHit(&weapon,&target) && deleted==1 && target.CType==INVALID_NODE_FLAG && hitHandlers==1);
    assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){0})==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetErrorCount()==0);PangeaScript_Shutdown();return 0;
}
