// Exercise the real native adapter callbacks without loading game assets.
#include <assert.h>
#include <string.h>
#define PANGEA_ENABLE_SCRIPTING 1
#include "../src/Scripting/ScriptBindings.c"

ObjNode* gPlayerObj;
short gNumLives;

int main(void)
{
    ObjNode object = {0};
    gPlayerObj = &object;
    assert(SetScriptPlayerLives(0, SHRT_MAX) == PANGEA_SCRIPT_OK);
    assert(SetScriptPlayerLives(0, SHRT_MAX + 1) == PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(gNumLives == SHRT_MAX);
    assert(SetScriptPlayerLives(0, -1) == PANGEA_SCRIPT_BAD_ARGUMENT);
    assert(SetScriptPlayerLives(1, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
    gPlayerObj = NULL;
    assert(SetScriptPlayerLives(0, 0) == PANGEA_SCRIPT_BAD_ARGUMENT);
    object.CType = CTYPE_MISC;
    const uint32_t mask = STATUS_BIT_HIDDEN | STATUS_BIT_NOCOLLISION;
    const uint32_t original = STATUS_BIT_NOCOLLISION;
    object.StatusBits = original;
    bool active = false;
    assert(BugdomScript_GetObjectActive(&object, &active) && active);
    assert(BugdomScript_SetObjectActive(&object, false));
    assert(BugdomScript_GetObjectActive(&object, &active) && !active);
    assert((object.StatusBits & mask) == mask);
    assert(BugdomScript_SetObjectActive(&object, false)); // Idempotence must not overwrite the saved state.
    assert(BugdomScript_SetObjectActive(&object, true));
    assert((object.StatusBits & mask) == original);

    assert(BugdomScript_SetObjectActive(&object, false));
    assert(BugdomScript_SetObjectCollisionEnabled(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION); // Inactive still cannot collide.
    assert(BugdomScript_SetObjectActive(&object, true));
    assert(!(object.StatusBits & STATUS_BIT_NOCOLLISION));
    object.StatusBits |= STATUS_BIT_DONTCULL;
    assert(BugdomScript_SetObjectActive(&object, false));
    assert(BugdomScript_SetObjectActive(&object, true));
    assert(object.StatusBits & STATUS_BIT_DONTCULL); // Unrelated native status survives.

    assert(BugdomScript_SetObjectActive(&object, false));
    assert(BugdomScript_SetObjectCollisionEnabled(&object, false));
    assert(BugdomScript_SetObjectActive(&object, true));
    assert(object.StatusBits & STATUS_BIT_NOCOLLISION);

    strcpy(object.ScriptDefinitionID, "test.custom");
    object.StatusBits = STATUS_BIT_HIDDEN;
    assert(BugdomScript_SetObjectActive(&object, false));
    assert(BugdomScript_SetObjectActive(&object, true));
    assert((object.StatusBits & mask) == STATUS_BIT_HIDDEN);

    object.ScriptDeleteRequested = true;
    assert(!BugdomScript_SetObjectActive(&object, false));
    assert(!BugdomScript_SetObjectCollisionEnabled(&object, true));
    object.ScriptDeleteRequested = false;
    object.CType = INVALID_NODE_FLAG;
    assert(!BugdomScript_SetObjectActive(&object, false));
    assert(!BugdomScript_SetObjectCollisionEnabled(&object, true));
    assert(!BugdomScript_SetObjectActive(NULL, false));
    return 0;
}
