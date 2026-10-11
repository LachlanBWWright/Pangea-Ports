#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
short gNumPlayersEliminated;
Boolean gTrackCompleted;
int gTrackNum,gGameMode;
OGLPoint3D gCoord;OGLVector3D gDelta;
Boolean IsRaceMode(void){return true;}
Boolean PangeaNet_IsHostAuthoritativeRemotePlayer(short p){(void)p;return false;}
float gTrackCompletedCoolDownTimer;
void NextLap(short p);
UInt32 TickCount(void){return 0;}
Boolean LevelMetadataProfileIs(const char* k,const char* p,Boolean f){(void)k;(void)p;return f;}
void PlayerDropFlag(ObjNode* p){(void)p;assert(0);}
void PlayAnnouncerSound(short e,Boolean o,float v){(void)e;(void)o;(void)v;assert(0);}
void ShowLapNum(short p){(void)p;}
void ShowFinalPlace(short p,int rank){(void)p;(void)rank;}
int SaveRaceTime(int p){(void)p;return 2;}
void ShowWinLose(short playerNum,Byte mode,short winner){(void)playerNum;(void)mode;(void)winner;}
void CalcObjectBoxFromNode(ObjNode* o){assert(o->CollisionBoxes);}
void UpdateObjectTransforms(ObjNode* o){(void)o;}
int main(void)
{
    PangeaScriptGameInfo info={.gameId="CroMagRally-Android",.gameName="Cro-Mag Rally",.capabilities=PANGEA_SCRIPT_CRO_MAG_RALLY_CAPABILITIES};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);
    assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native_gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={.Coord={10,20,30}};gPlayerInfo[0].objNode=&player;gPlayerInfo[0].health=1;gNumTotalPlayers=1;gTrackNum=4;
    static const PangeaScriptObjectOps ops={.getPosition=CroMagScript_GetObjectPosition};
    PangeaScriptObjectRegistration registration={.nativeObject=&player,.ops=&ops,.objectType="player",.capabilityLevel=PANGEA_SCRIPT_CAPABILITY_FULL};
    PangeaScriptObjectHandle handle;assert(PangeaScript_RegisterObject(&registration,&handle)==PANGEA_SCRIPT_OK);player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
 CroMagScript_ApplyObjectScripting(&player); /* Decorative objects may have no collision boxes. */
    ObjNode weapon={0};BlastCarsWithWeapon(0,0,0,0,100,&weapon,1);assert(player.Delta.y==0);
    BlastCarsWithWeapon(0,0,0,0,100,&weapon,1);assert(player.Delta.y>0);
    PlayerLoseHealth(0,.1f);assert(gPlayerInfo[0].health==1); /* Lua suppression */
    PlayerLoseHealth(0,.2f);assert(gPlayerInfo[0].health==.75f); /* Lua replacement */
    PlayerLoseHealth(0,2);assert(gPlayerInfo[0].health==0 && gPlayerInfo[0].isEliminated && gNumPlayersEliminated==1);
    gNumCheckpoints=2;gCheckpointList[0]=(CheckpointDefType){.x={-10,-10},.z={-2,2}};
    gCheckpointList[1]=(CheckpointDefType){.x={5,5},.z={-2,2}};player.OldCoord=(OGLPoint3D){0,0,0};gCoord=(OGLPoint3D){10,0,0};
    UpdatePlayerCheckpoints(0);assert(gPlayerInfo[0].checkpointNum==1 && gPlayerInfo[0].checkpointTagged[1]);
    gPlayerInfo[0].lapNum=0;gNumLapsThisRace=2;NextLap(0);assert(gPlayerInfo[0].lapNum==1 && !gPlayerInfo[0].raceComplete);
    NextLap(0);assert(gPlayerInfo[0].raceComplete && gTrackCompleted);PlayerCompletedRace(0);
    assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){.levelNum=4})==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetStatusErrorCount()==0);PangeaScript_Shutdown();puts("native Cro-Mag Lua weapon, checkpoint, lap, finish, damage and death passed");return 0;
}
