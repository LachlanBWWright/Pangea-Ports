#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
int gCurrentArea;
static int morphs;
extern float gDeathTimer;
void MorphToSkeletonAnim(SkeletonObjDataType* skeleton,long animNum,float speed){(void)skeleton;(void)animNum;(void)speed;morphs++;}
void CalcObjectBoxFromNode(ObjNode* o){assert(o->CollisionBoxes);}
void UpdateObjectTransforms(ObjNode* o){(void)o;}
int main(void)
{
    PangeaScriptGameInfo info={.gameId="BillyFrontier-Android",.gameName="Billy Frontier",.capabilities=PANGEA_SCRIPT_BILLY_FRONTIER_CAPABILITIES};
    assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native_gameplay.lua")==PANGEA_SCRIPT_OK);
    ObjNode player={.Coord={10,20,30},.Health=1};gPlayerInfo.objNode=&player;gCurrentArea=3;
    static const PangeaScriptObjectOps ops={.getPosition=BillyScript_GetObjectPosition};
    PangeaScriptObjectRegistration registration={.nativeObject=&player,.ops=&ops,.objectType="player",.capabilityLevel=PANGEA_SCRIPT_CAPABILITY_FULL};
    PangeaScriptObjectHandle handle;assert(PangeaScript_RegisterObject(&registration,&handle)==PANGEA_SCRIPT_OK);player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
 BillyScript_ApplyObjectScripting(&player); /* Decorative objects may have no collision boxes. */
    KillPlayer(PLAYER_DEATH_TYPE_TRAMPLED);assert(gPlayerIsDead && player.Health==0 && gDeathTimer==3 && morphs==1);
    KillPlayer(PLAYER_DEATH_TYPE_TRAMPLED);assert(morphs==1); /* Native guard suppresses duplicate death. */
    assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){.levelNum=3})==PANGEA_SCRIPT_OK);
    assert(PangeaScript_GetStatusErrorCount()==0);PangeaScript_Shutdown();puts("native Billy death/duplicate guard through Lua passed");return 0;
}
