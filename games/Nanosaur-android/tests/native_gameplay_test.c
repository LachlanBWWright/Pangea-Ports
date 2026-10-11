// Real production trigger consumer and adapter, executed by the real Lua runtime.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../src/Scripting/ScriptBindings.c"
#include "../src/Items/Triggers.c"
#include "../src/Player/MyGuy.c"
#include "../src/Enemies/Enemy.c"

float gMyHealth;
uint32_t gScore;
uint32_t gInfobarUpdateBits;
Boolean gPlayerGotKilledFlag, gGameOverFlag;
short gNumLives;
int EXPLODEGEOMETRY_DENOMINATOR=1;
float gFuel;
static int explosions, removed;
void QD3D_ExplodeGeometry(ObjNode* o,float f,Byte m,int d,float s){(void)o;(void)f;(void)m;(void)d;(void)s;explosions++;}
ObjNode* MakeTimePortal(Byte t,float x,float z){(void)t;(void)x;(void)z;static ObjNode portal;return &portal;}
short PlayEffect_Parms(short e,unsigned char v,unsigned long f){(void)e;(void)v;(void)f;return -1;}
void UpdateObjectTransforms(ObjNode* o){(void)o;}
void ResetCameraSettings(void){}
void InitWeaponManager(void){}
void AddToScore(long score){gScore+=score;}
void DeleteObject(ObjNode* o){o->CType=INVALID_NODE_FLAG;removed++;}
static int jetStops, deathAnimations;
void StopJetPack(ObjNode* node) {(void)node; jetStops++;}
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton, long animation, float speed) {(void)skeleton;(void)animation;(void)speed;deathAnimations++;}
void SetSkeletonAnim(SkeletonObjDataType* skeleton, long animation) {(void)skeleton;(void)animation;}
static int sounds;
short PlayEffect(short effect) { (void)effect; sounds++; return 0; }

static int collisionUpdates;
void CalcObjectBoxFromNode(ObjNode* o){assert(o->CollisionBoxes && o->NumCollisionBoxes==1);collisionUpdates++;}

static char* fixture_text(const char* path, long* outSize)
{
    if (strcmp(path, "Data/Scripts/dist/native-gameplay.lua")) return NULL;
    const char* text = "local n,d,a,dead,respawn,reset,w=0,0,0,0,0,0,0; return { onDamage=function(ctx) d=d+1; assert(math.abs(ctx.damage-0.2)<0.0001); if d==1 then return {applyDamage=false} end; return {damage=d==2 and 0.1 or 2} end, onDamageApplied=function(ctx) a=a+1; assert(math.abs(ctx.damage-(a==1 and 0.1 or 2))<0.0001) end, onPlayerRespawn=function() respawn=respawn+1 end,onObjectFrame=function(ctx) if ctx.event=='checkpointReset' then reset=reset+1 end end,onWeaponHit=function(ctx) w=w+1; if w==1 then return {applyDamage=false} elseif w==2 then return {damage=0.1} else return {applyDamage=false,destroyTarget=true} end end, onDeath=function() dead=dead+1 end, onPickupCollected=function(ctx) n=n+1; assert(ctx.pickupId=='nanosaur.crystal'); assert(ctx.amount==1); return {consumePickup=n>1,healthDelta=0.1,scoreDelta=7} end, onFrame=function() assert(n==2 and d==3 and a==2 and dead==1 and respawn==1 and reset==1 and w==3) end }";
    *outSize = (long)strlen(text);
    return strdup(text);
}
int main(void)
{
    PangeaScriptGameInfo info = {.gameId="Nanosaur",.gameName="Nanosaur",.readTextFile=fixture_text};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native-gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={0}, pickup={0}; TerrainItemEntryType terrain={0};
    pickup.TerrainItemPtr=&terrain; gMyHealth=.5f;
    assert(!DoTrig_Crystal(&pickup,&player,0));
    assert(pickup.TerrainItemPtr==&terrain && sounds==0 && gScore==7);
    assert(DoTrig_Crystal(&pickup,&player,0));
    assert(pickup.TerrainItemPtr==NULL && sounds==1 && gScore==14);
    assert(fabsf(gMyHealth-.7f)<.0001f);
    PangeaScriptObjectHandle handle={0};
    static const PangeaScriptObjectOps ops={.getPosition=NanosaurScript_GetObjectPosition};
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&player,.ops=&ops,.objectType="player"},&handle)==PANGEA_SCRIPT_OK);
    player.ScriptObjectID=handle.id; player.ScriptObjectGeneration=handle.generation; gPlayerObj=&player;
    PlayerGotHurt(&player,.2f,false,true);
    assert(fabsf(gMyHealth-.7f)<.0001f && jetStops==0 && player.InvincibleTimer==0);
    PlayerGotHurt(&player,.2f,false,true);
    assert(fabsf(gMyHealth-.6f)<.0001f && jetStops==1 && !gPlayerGotKilledFlag);
    player.InvincibleTimer=0;
    PlayerGotHurt(&player,.2f,false,true);
    assert(gPlayerGotKilledFlag && jetStops==3 && deathAnimations==1);
    KillPlayer(&player); // Native duplicate-death guard must suppress duplicate Lua event.
    assert(deathAnimations==1);
    gNumLives=2; ResetPlayer();
    assert(gNumLives==1 && !gPlayerGotKilledFlag && gMyHealth==1 && gFuel==0);
    assert(player.StatusBits & STATUS_BIT_HIDDEN);
    ObjNode enemy={.Health=1,.Kind=0}, weapon={0};
    PangeaScriptObjectHandle enemyHandle, weaponHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&enemy,.ops=&ops,.objectType="enemy"},&enemyHandle)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&weapon,.ops=&ops,.objectType="weapon"},&weaponHandle)==PANGEA_SCRIPT_OK);
    enemy.ScriptObjectID=enemyHandle.id;enemy.ScriptObjectGeneration=enemyHandle.generation;
    weapon.ScriptObjectID=weaponHandle.id;weapon.ScriptObjectGeneration=weaponHandle.generation;
    InitEnemyManager();gNumEnemies=1;gNumEnemyOfKind[0]=1;
    assert(!EnemyGotHurt(&enemy,&weapon,.5f) && enemy.Health==1);
    assert(!EnemyGotHurt(&enemy,&weapon,.5f) && fabsf(enemy.Health-.9f)<.0001f);
    assert(EnemyGotHurt(&enemy,&weapon,.5f) && removed==1 && gNumEnemies==0 && gNumEnemyOfKind[0]==0);
    ObjNode decorative={0};PangeaScriptObjectHandle decorativeHandle;
    assert(PangeaScript_RegisterObject(&(PangeaScriptObjectRegistration){.nativeObject=&decorative,.ops=&ops,.objectType="decorative"},&decorativeHandle)==PANGEA_SCRIPT_OK);
    decorative.ScriptObjectID=decorativeHandle.id;decorative.ScriptObjectGeneration=decorativeHandle.generation;
    NanosaurScript_ApplyObjectScripting(&decorative);
    assert(collisionUpdates==0); // Native decorative objects do not own a collision box.
    assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){0})==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetErrorCount()==0);
    PangeaScript_Shutdown();
    return 0;
}
