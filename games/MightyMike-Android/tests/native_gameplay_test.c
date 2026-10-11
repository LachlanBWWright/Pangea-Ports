#include "../src/Scripting/ScriptBindings.c"
#include <assert.h>
void IGotHurt(void);
void MeHitBonusObject(ObjNode* o);
Byte gSceneNum,gAreaNum;
short gMyHealth,gShieldTimer;
ObjNode* gShieldNodePtr;
short gMyMaxHealth;long gScore;Boolean gSpaceShipFlag;
void GetAWeapon(short w){(void)w;assert(0);}
void GetMiscPOW(short p){(void)p;assert(0);}
void GiveMeHealth(void){assert(0);}
void GetCoins(short n){(void)n;assert(0);}
void DeleteBunny(ObjNode* b){(void)b;assert(0);}
void DecBunnyCount(void){assert(0);}
void TurnMeIntoShip(ObjNode* s){(void)s;assert(0);}
void ShowKeys(void){}
int LevelMetadataScene(const char* key,int fallback){(void)key;return fallback;}
void DisposeFrog(void){}
void DisposeSpaceShip(void){}
void ShowHealth(void){}
void SwitchAnim(ObjNode* o,short a){(void)o;(void)a;}
void MakeMikeMessage(short m){(void)m;}
short PlaySound(short s){(void)s;return -1;}
static int deleted;
void DeleteObject(ObjNode* o){(void)o;deleted++;}

int main(void)
{
 PangeaScriptGameInfo info={.gameId="MightyMike-Android",.gameName="Mighty Mike",.capabilities=PANGEA_SCRIPT_MIGHTY_MIKE_CAPABILITIES};
 assert(PangeaScript_Init(&info)==PANGEA_SCRIPT_OK);assert(PangeaScript_SetStartupScript("Data/Scripts/dist/native_gameplay.lua")==PANGEA_SCRIPT_OK);
 ObjNode player={.CType=1};gMyNodePtr=&player;gMyHealth=4;gMyMaxHealth=4;gSceneNum=1;gAreaNum=1;
 static const PangeaScriptObjectOps ops={.getPosition=MikeScript_GetObjectPosition};
 PangeaScriptObjectRegistration registration={.nativeObject=&player,.ops=&ops,.objectType="player",.capabilityLevel=PANGEA_SCRIPT_CAPABILITY_FULL};
 PangeaScriptObjectHandle handle;assert(PangeaScript_RegisterObject(&registration,&handle)==PANGEA_SCRIPT_OK);player.ScriptObjectID=handle.id;player.ScriptObjectGeneration=handle.generation;
 IGotHurt();assert(gMyHealth==4);
 IGotHurt();assert(gMyHealth==2 && gMyBlinkieTimer>0);
 ObjNode key={.CType=CTYPE_KEY,.SubType=0};
 MeHitBonusObject(&key);assert(!gMyKeys[0] && deleted==0);
 MeHitBonusObject(&key);assert(gMyKeys[0] && deleted==1);
 assert(PangeaScript_CallFrameHook(&(PangeaScriptFrameContext){.levelNum=4})==PANGEA_SCRIPT_OK);
 assert(PangeaScript_GetStatusErrorCount()==0);PangeaScript_Shutdown();puts("native MightyMike Lua damage and key pickup passed");return 0;
}
