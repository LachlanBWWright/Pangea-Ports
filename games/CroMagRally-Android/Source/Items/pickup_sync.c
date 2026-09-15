#include "game.h"
#include "network.h"
#include "pickup_sync.h"
#include <limits.h>
#include <string.h>

// Terrain IDs survive local object streaming; ObjNode pointers do not.
static Boolean gPickupHidden[SHRT_MAX + 1];
static float gPickupCooldown[SHRT_MAX + 1];

static int GetPickupIndex(const ObjNode* node)
{
	if (!node->TerrainItemPtr || !gMasterItemList || !*gMasterItemList)
		return -1;
	const ptrdiff_t index = node->TerrainItemPtr - *gMasterItemList;
	return index >= 0 && index < gNumTerrainItems ? (int)index : -1;
}

void PangeaPickup_Reset(void)
{
	memset(gPickupHidden, 0, sizeof(gPickupHidden));
	memset(gPickupCooldown, 0, sizeof(gPickupCooldown));
}

Boolean PangeaPickup_CanCollect(ObjNode* node)
{
	if (!gIsNetworkHost && !gIsNetworkClient)
		return true;
	if (gIsNetworkClient)
		return false;
	const int index = GetPickupIndex(node);
	return index < 0 || !gPickupHidden[index];
}

void PangeaPickup_Collect(ObjNode* node)
{
	if (!gIsNetworkHost)
		return;
	const int index = GetPickupIndex(node);
	if (index < 0 || gPickupHidden[index])
		return;
	gPickupHidden[index] = true;
	gPickupCooldown[index] = 5.0f;
	PangeaNet_SendPickupState((uint16_t)index, true);
}

void PangeaPickup_Update(void)
{
	if (!gIsNetworkHost)
		return;
	for (int i = 0; i < gNumTerrainItems; i++)
	{
		if (!gPickupHidden[i])
			continue;
		gPickupCooldown[i] -= gFramesPerSecondFrac;
		if (gPickupCooldown[i] > 0)
			continue;
		gPickupHidden[i] = false;
		PangeaNet_SendPickupState((uint16_t)i, false);
	}
}

void PangeaPickup_ReceiveState(uint16_t itemIndex, Boolean hidden)
{
	if (gIsNetworkClient && itemIndex < gNumTerrainItems)
		gPickupHidden[itemIndex] = hidden;
}

Boolean PangeaPickup_ApplyObject(ObjNode* node)
{
	if (!gIsNetworkHost && !gIsNetworkClient)
		return false;
	const int index = GetPickupIndex(node);
	if (index < 0)
		return false;
	const Boolean hidden = gPickupHidden[index];
	node->Flag[0] = hidden;
	node->CType = hidden ? 0 : CTYPE_TRIGGER;
	if (hidden)
		node->StatusBits |= STATUS_BIT_HIDDEN;
	else
		node->StatusBits &= ~STATUS_BIT_HIDDEN;
	if (node->ShadowNode)
	{
		if (hidden)
			node->ShadowNode->StatusBits |= STATUS_BIT_HIDDEN;
		else
			node->ShadowNode->StatusBits &= ~STATUS_BIT_HIDDEN;
	}
	return true;
}
