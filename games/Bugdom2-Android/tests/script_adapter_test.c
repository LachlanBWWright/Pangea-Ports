// Exercise the real native adapter callbacks without loading game assets.
#include <assert.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../Source/Scripting/ScriptBindings.c"

int main(void)
{
    ObjNode object = {0};
    object.CType = CTYPE_MISC;
    const uint32_t mask = STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE;
    const uint32_t original = STATUS_BIT_NOCOLLISION | STATUS_BIT_NOMOVE;
    object.StatusBits = original;
    bool active = false;
    assert(Bugdom2Script_GetObjectActive(&object, &active) && active);
    assert(Bugdom2Script_SetObjectActive(&object, false));
    assert(Bugdom2Script_GetObjectActive(&object, &active) && !active);
    assert((object.StatusBits & mask) == mask);
    assert(Bugdom2Script_SetObjectActive(&object, false)); // Idempotence must not overwrite the saved state.
    assert(Bugdom2Script_SetObjectActive(&object, true));
    assert((object.StatusBits & mask) == original);

    assert(Bugdom2Script_SetObjectActive(&object, false));
    assert(Bugdom2Script_SetObjectCollisionEnabled(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION); // Inactive still cannot collide.
    assert(Bugdom2Script_SetObjectActive(&object, true));
    assert(!(object.StatusBits & STATUS_BIT_NOCOLLISION));
    assert(object.StatusBits & STATUS_BIT_NOMOVE); // Originally static objects remain static.

    assert(Bugdom2Script_SetObjectActive(&object, false));
    assert(Bugdom2Script_SetObjectCollisionEnabled(&object, false));
    assert(Bugdom2Script_SetObjectActive(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION);

    strcpy(object.ScriptDefinitionID, "test.custom");
    object.StatusBits = STATUS_BIT_HIDDEN;
    assert(Bugdom2Script_SetObjectActive(&object, false));
    assert(Bugdom2Script_SetObjectActive(&object, true));
    assert((object.StatusBits & mask) == STATUS_BIT_HIDDEN);

    object.ScriptDeleteRequested = true;
    assert(!Bugdom2Script_SetObjectActive(&object, false));
    assert(!Bugdom2Script_SetObjectCollisionEnabled(&object, true));
    object.ScriptDeleteRequested = false;
    object.CType = INVALID_NODE_FLAG;
    assert(!Bugdom2Script_SetObjectActive(&object, false));
    assert(!Bugdom2Script_SetObjectCollisionEnabled(&object, true));
    assert(!Bugdom2Script_SetObjectActive(NULL, false));
    return 0;
}
