#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
short gNumTotalPlayers;
PlayerInfoType gPlayerInfo[MAX_PLAYERS];
int main(void)
{
    ObjNode player={0};gNumTotalPlayers=1;gPlayerInfo[0].objNode=&player;
    gPlayerInfo[0].powType=POW_TYPE_TORPEDO;gPlayerInfo[0].powQuantity=5;
    assert(SetScriptPlayerWeaponQuantity(0,POW_TYPE_BONE,0)==PANGEA_SCRIPT_OK);
    assert(gPlayerInfo[0].powType==POW_TYPE_TORPEDO && gPlayerInfo[0].powQuantity==5);
    assert(SetScriptPlayerWeaponQuantity(0,POW_TYPE_TORPEDO,0)==PANGEA_SCRIPT_OK);
    assert(gPlayerInfo[0].powType==POW_TYPE_NONE && gPlayerInfo[0].powQuantity==0);
    assert(SetScriptPlayerWeaponQuantity(0,POW_TYPE_BONE,4)==PANGEA_SCRIPT_OK);
    assert(gPlayerInfo[0].powType==POW_TYPE_BONE && gPlayerInfo[0].powQuantity==4);
    ObjNode native={.CType=1,.StatusBits=STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION};
    bool active;
    assert(CroMagScript_SetObjectActive(&native,false));
    assert(CroMagScript_GetObjectActive(&native,&active) && !active);
    assert(native.StatusBits & STATUS_BIT_NOMOVE);
    assert(CroMagScript_SetObjectCollisionEnabled(&native,true));
    assert(native.StatusBits & STATUS_BIT_NOCOLLISION);
    assert(CroMagScript_SetObjectActive(&native,true));
    assert(CroMagScript_GetObjectActive(&native,&active) && active);
    assert(native.StatusBits & STATUS_BIT_HIDDEN);
    assert(!(native.StatusBits & (STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE)));
    return 0;
}
