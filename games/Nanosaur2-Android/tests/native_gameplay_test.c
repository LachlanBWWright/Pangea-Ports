#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
short gLevelNum,gVSMode;
PrefsType gGamePrefs;
Boolean gCameraInDeathDiveMode[MAX_PLAYERS];
NewParticleGroupDefType gNewParticleGroupDef;
NewConfettiGroupDefType gNewConfettiGroupDef;
OGLPoint3D gCoord,gBestCheckpointCoord[MAX_PLAYERS];
float gBestCheckpointAim[MAX_PLAYERS],gTargetMaxSpeed[MAX_PLAYERS],gCurrentMaxSpeed[MAX_PLAYERS];
Boolean gGameOver;
Boolean LevelMetadataProfileIs(const char* k,const char* p,Boolean fallback){(void)k;(void)p;return fallback;}
float OGLVector3D_Dot(const OGLVector3D* a,const OGLVector3D* b){return a->x*b->x+a->y*b->y+a->z*b->z;}
float GetTerrainY(float x,float z){(void)x;(void)z;return 0;}
void DoPlayerTerrainUpdate(void){}
void SetPlayerFlyingAnim(ObjNode* p){(void)p;}
void InitCamera_Terrain(short p){(void)p;}
ObjNode* MakeFadeEvent(Byte f,float s){(void)f;(void)s;return NULL;}
void UpdateObjectTransforms(ObjNode* o){(void)o;}
void MorphToSkeletonAnim(SkeletonObjDataType* o,long a,float s){(void)o;(void)a;(void)s;}
void SetSkeletonAnim(SkeletonObjDataType* o,long a){(void)o;(void)a;}
void DeleteObject(ObjNode* o){(void)o;}
void StopAChannel(short* c){(void)c;}
void JetpackOff(short p){(void)p;}
void DropEgg_NoWormhole(short p){(void)p;}
void PlayRumbleEffect(short e,int p){(void)e;(void)p;}
short PlayEffect3D(short e,OGLPoint3D* p){(void)e;(void)p;return -1;}
short PlayEffect_Parms3D(short e,OGLPoint3D* p,uint32_t r,float v){(void)e;(void)p;(void)r;(void)v;return -1;}
ObjNode* ShowWinLose(short p,Byte m){(void)p;(void)m;return NULL;}
void StartLevelCompletion(float t){(void)t;}
short NewParticleGroup(NewParticleGroupDefType* d){(void)d;return -1;}
Boolean AddParticleToGroup(const NewParticleDefType* d){(void)d;return false;}
short NewConfettiGroup(NewConfettiGroupDefType* d){(void)d;return -1;}
Boolean AddConfettiToGroup(NewConfettiDefType* d){(void)d;return false;}
float RandomFloat(void){return .5f;}
float RandomFloat2(void){return 0;}

void CalcObjectBoxFromNode(ObjNode* o){assert(o->CollisionBoxes);}
int main(void)
{
 PangeaScriptGameInfo info={.gameId="Nanosaur2-Android",.gameName="Nanosaur 2",.capabilities=PANGEA_SCRIPT_NANOSAUR2_CAPABILITIES};
 assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native_gameplay.lua")==PANGEA_SCRIPT_OK);
 ObjNode player={.Coord={10,20,30}};gPlayerInfo[0].objNode=&player;gPlayerInfo[0].health=1;gNumPlayers=1;gLevelNum=4;
 static const PangeaScriptObjectOps ops={.getPosition=Nanosaur2Script_GetObjectPosition};
 PangeaScriptObjectRegistration registration={.nativeObject=&player,.ops=&ops,.objectType="player",.capabilityLevel=PANGEA_SCRIPT_CAPABILITY_FULL};
 PangeaScriptObjectHandle handle;assert(PangeaScript_RegisterObject(&registration,&handle)==PANGEA_SCRIPT_OK);player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
 Nanosaur2Script_ApplyObjectScripting(&player); /* Decorative objects may have no collision boxes. */
 assert(!PlayerLoseHealth(0,.1f,0,NULL,false));assert(gPlayerInfo[0].health==1);
 assert(!PlayerLoseHealth(0,.2f,0,NULL,false));assert(gPlayerInfo[0].health==.75f);
 assert(PlayerLoseHealth(0,2,PLAYER_DEATH_TYPE_DEATHDIVE,NULL,false));assert(gPlayerIsDead[0] && gPlayerInfo[0].health==0 && gCameraInDeathDiveMode[0]);
 KillPlayer(0,PLAYER_DEATH_TYPE_DEATHDIVE,NULL);
 gPlayerInfo[0].numFreeLives=2;gBestCheckpointCoord[0]=(OGLPoint3D){100,200,300};
 ResetPlayerAtBestCheckpoint(0);assert(!gPlayerIsDead[0] && gPlayerInfo[0].health==1 && gPlayerInfo[0].numFreeLives==1 && player.Coord.x==100 && player.Coord.z==300);
 assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){.levelNum=4})==PANGEA_SCRIPT_OK);
 assert(PangeaScript_GetStatusErrorCount()==0);PangeaScript_Shutdown();puts("native Nanosaur2 Lua damage, death and checkpoint respawn passed");return 0;
}
