#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
Byte gNumPlayers;
PlayerInfoType gPlayerInfo[MAX_PLAYERS];
static int selectionCalls;
void SelectNextWeapon(short playerNum, Boolean allowSonicScream, int delta)
{
    assert(playerNum==0 && allowSonicScream && delta==1);
    selectionCalls++;
    gPlayerInfo[playerNum].currentWeapon=WEAPON_TYPE_SONICSCREAM;
}
int main(void)
{
    ObjNode player={0};gNumPlayers=1;gPlayerInfo[0].objNode=&player;
    assert(SetScriptPlayerLives(0,INT16_MAX)==PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerLives(0,INT16_MAX+1)==PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gPlayerInfo[0].numFreeLives==INT16_MAX);
    gPlayerInfo[0].currentWeapon=WEAPON_TYPE_BLASTER;
    assert(SetScriptPlayerWeaponQuantity(0,WEAPON_TYPE_BLASTER,0)==PANGEA_SCRIPT_OK);
    assert(selectionCalls==1 && gPlayerInfo[0].currentWeapon==WEAPON_TYPE_SONICSCREAM);
    assert(SetScriptPlayerWeaponQuantity(0,WEAPON_TYPE_BLASTER,10)==PANGEA_SCRIPT_OK);
    assert(selectionCalls==1 && gPlayerInfo[0].weaponQuantity[WEAPON_TYPE_BLASTER]==10);
    ObjNode native={.CType=1,.StatusBits=STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION};
    bool active;
    assert(Nanosaur2Script_SetObjectActive(&native,false));
    assert(Nanosaur2Script_GetObjectActive(&native,&active) && !active);
    assert(native.StatusBits & STATUS_BIT_NOMOVE);
    assert(Nanosaur2Script_SetObjectCollisionEnabled(&native,true));
    assert(native.StatusBits & STATUS_BIT_NOCOLLISION);
    assert(Nanosaur2Script_SetObjectActive(&native,true));
    assert(Nanosaur2Script_GetObjectActive(&native,&active) && active);
    assert(native.StatusBits & STATUS_BIT_HIDDEN);
    assert(!(native.StatusBits & (STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE)));
    return 0;
}
