#include "../src/Scripting/ScriptBindings.c"
#include <assert.h>
ObjNode* gMyNodePtr;
WeaponType gMyWeapons[MAX_WEAPONS];
Byte gCurrentWeaponType,gNumWeaponsIHave,gCurrentWeaponIndex;
short gNumLives;
long gMyX,gMyY,gMyDX,gMyDY,gMySumDX,gMySumDY;
void ShowLives(void) {}
void CalcObjectBox2(ObjNode* obj) { (void)obj; }
int main(void)
{
    ObjNode player={0};gMyNodePtr=&player;
    assert(SetScriptPlayerLives(0,INT16_MAX)==PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerLives(0,INT16_MAX+1)==PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gNumLives==INT16_MAX);
    gNumWeaponsIHave=3;gCurrentWeaponIndex=1;
    gMyWeapons[0]=(WeaponType){1,10};gMyWeapons[1]=(WeaponType){2,20};gMyWeapons[2]=(WeaponType){3,30};
    gCurrentWeaponType=2;
    assert(SetScriptPlayerWeaponQuantity(0,1,0)==PANGEA_SCRIPT_OK);
    assert(gNumWeaponsIHave==2 && gCurrentWeaponIndex==0 && gCurrentWeaponType==2);
    assert(SetScriptPlayerWeaponQuantity(0,1,0)==PANGEA_SCRIPT_OK);
    assert(gNumWeaponsIHave==2 && gCurrentWeaponType==2);
    PangeaScriptVector3 pos={32767.998f,-32768.0f,0};
    assert(MikeScript_SetObjectPosition(&player,&pos));
    int32_t oldX=player.X.L,oldY=player.Y.L;
    pos=(PangeaScriptVector3){32768,0,0};assert(!MikeScript_SetObjectPosition(&player,&pos));
    assert(player.X.L==oldX && player.Y.L==oldY);
    pos=(PangeaScriptVector3){0,NAN,0};assert(!MikeScript_SetObjectVelocity(&player,&pos));
    pos=(PangeaScriptVector3){1,2,0};assert(MikeScript_SetObjectVelocity(&player,&pos));
    assert(player.DX==65536 && player.DY==131072);
    ObjNode native={.CType=1,.CBits=7,.DrawFlag=false,.MoveFlag=true};
    assert(MikeScript_SetObjectActive(&native,false));
    assert(!native.DrawFlag && !native.MoveFlag && native.CBits==0);
    assert(MikeScript_SetObjectCollisionEnabled(&native,false));
    assert(MikeScript_SetObjectActive(&native,true));
    assert(!native.DrawFlag && native.MoveFlag && native.CBits==0);
    assert(MikeScript_SetObjectCollisionEnabled(&native,true));
    assert(native.CBits==7);
    return 0;
}
