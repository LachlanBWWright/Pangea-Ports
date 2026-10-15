static ObjNode* gScriptPickup;
static void (*gScriptPickupMove)(ObjNode*);
static uint32_t gScriptPickupCollision;

static void SaveScriptPickup(ObjNode* pickup)
{
	gScriptPickup = pickup;
	gScriptPickupMove = pickup->MoveCall;
	gScriptPickupCollision = pickup->CType;
}

void OttoScript_RestoreNativePickup(ObjNode* pickup)
{
	if (!pickup) return;
	pickup->MoveCall = pickup == gScriptPickup ? gScriptPickupMove : MovePowerup;
	pickup->CType = pickup == gScriptPickup ? gScriptPickupCollision : CTYPE_POWERUP | CTYPE_MISC;
	gScriptPickup = NULL;
}

void OttoScript_ForgetNativePickup(ObjNode* pickup)
{
	if (pickup == gScriptPickup) gScriptPickup = NULL;
}
