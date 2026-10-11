// Exercise the real native adapter callbacks without loading game assets.
#include <assert.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../src/Scripting/ScriptBindings.c"

PlayerInfoType gPlayerInfo;

int main(void)
{
    ObjNode object = {0};
    gPlayerInfo.objNode = &object;
    assert(SetScriptPlayerLives(0, UINT8_MAX) == PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerLives(0, UINT8_MAX + 1) == PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gPlayerInfo.lives == UINT8_MAX);
    assert(SetScriptPlayerLives(0, -1) == PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(SetScriptPlayerLives(1, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
    gPlayerInfo.objNode = NULL;
    assert(SetScriptPlayerLives(0, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
    object.CType = CTYPE_MISC;
    const uint32_t mask = STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE;
    const uint32_t original = STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE;
    object.StatusBits = original;
    bool active = false;
    assert(OttoObjectGetActive(&object, &active) && active);
    assert(OttoObjectSetActive(&object, false));
    assert(OttoObjectGetActive(&object, &active) && !active);
    assert((object.StatusBits & mask) == mask);
    assert(OttoObjectSetActive(&object, false)); // Idempotence must not overwrite the saved state.
    assert(OttoObjectSetActive(&object, true));
    assert((object.StatusBits & mask) == original);

    assert(OttoObjectSetActive(&object, false));
    assert(OttoObjectSetCollisionEnabled(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION); // Inactive still cannot collide.
    assert(OttoObjectSetActive(&object, true));
    assert(!(object.StatusBits & STATUS_BIT_NOCOLLISION));
    assert(object.StatusBits & STATUS_BIT_NOMOVE); // Originally static objects remain static.

    assert(OttoObjectSetActive(&object, false));
    assert(OttoObjectSetCollisionEnabled(&object, false));
    assert(OttoObjectSetActive(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION);

    strcpy(object.ScriptDefinitionID, "test.custom");
    object.StatusBits = STATUS_BIT_HIDDEN;
    assert(OttoObjectSetActive(&object, false));
    assert(OttoObjectSetActive(&object, true));
    assert((object.StatusBits & mask) == STATUS_BIT_HIDDEN);

    object.ScriptDeleteRequested = true;
    assert(!OttoObjectSetActive(&object, false));
    assert(!OttoObjectSetCollisionEnabled(&object, true));
    object.ScriptDeleteRequested = false;
    object.CType = INVALID_NODE_FLAG;
    assert(!OttoObjectSetActive(&object, false));
    assert(!OttoObjectSetCollisionEnabled(&object, true));
    assert(!OttoObjectSetActive(NULL, false));
    return 0;
}
