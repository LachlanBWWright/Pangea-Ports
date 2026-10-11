#include "../Source/Scripting/ScriptBindings.c"
#include <assert.h>
PlayerInfoType gPlayerInfo;
int main(void)
{
    ObjNode player={0};gPlayerInfo.objNode=&player;
    assert(SetScriptPlayerLives(0,INT8_MAX)==PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerLives(0,INT8_MAX+1)==PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gPlayerInfo.lives==INT8_MAX);
    assert(SetScriptPlayerWeaponQuantity(0,0,999)==PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerWeaponQuantity(0,0,1000)==PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gPlayerInfo.ammoCount==999);
    ObjNode native={.CType=0,.StatusBits=STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION};
    bool active;
    assert(BillyScript_SetObjectActive(&native,false));
    assert(BillyScript_GetObjectActive(&native,&active) && !active);
    assert(native.StatusBits & STATUS_BIT_NOMOVE);
    assert(BillyScript_SetObjectCollisionEnabled(&native,true));
    assert(native.StatusBits & STATUS_BIT_NOCOLLISION);
    assert(BillyScript_SetObjectActive(&native,true));
    assert(BillyScript_GetObjectActive(&native,&active) && active);
    assert(native.StatusBits & STATUS_BIT_HIDDEN);
    assert(!(native.StatusBits & (STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE)));
    return 0;
}
