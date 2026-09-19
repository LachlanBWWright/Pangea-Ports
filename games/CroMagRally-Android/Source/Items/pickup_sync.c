#include "game.h"
#include "network.h"
#include "pickup_sync.h"
#include <limits.h>
#include <string.h>

// Terrain IDs survive local object streaming; ObjNode pointers do not.
static Boolean gPickupHidden[SHRT_MAX + 1];
static float gPickupCooldown[SHRT_MAX + 1];
#define PICKUP_SNAPSHOT_BYTES 512

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

void PangeaPickup_CollectPermanent(ObjNode* node)
{
	if (!gIsNetworkHost)
		return;
	const int index = GetPickupIndex(node);
	if (index < 0 || gPickupHidden[index])
		return;
	gPickupHidden[index] = true;
	gPickupCooldown[index] = -1.0f;
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
		if (gPickupCooldown[i] < 0.0f)
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
	{
		gPickupHidden[itemIndex] = hidden;
		for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		{
			if (GetPickupIndex(node) != (int)itemIndex)
				continue;
			node->CType = hidden ? 0 : CTYPE_TRIGGER;
			if (hidden)
				node->StatusBits |= STATUS_BIT_HIDDEN;
			else
				node->StatusBits &= ~STATUS_BIT_HIDDEN;
		}
	}
}

int PangeaPickup_WriteSnapshotState(uint8_t* bytes, int maxBytes)
{
	if (!bytes || maxBytes < PICKUP_SNAPSHOT_BYTES)
		return 0;
	memset(bytes, 0, PICKUP_SNAPSHOT_BYTES);
	const int count = gNumTerrainItems < PICKUP_SNAPSHOT_BYTES * 8 ? gNumTerrainItems : PICKUP_SNAPSHOT_BYTES * 8;
	for (int i = 0; i < count; i++)
		if (gPickupHidden[i])
			bytes[i >> 3] |= (uint8_t)(1u << (i & 7));
	return PICKUP_SNAPSHOT_BYTES;
}

void PangeaPickup_ReceiveSnapshotState(const uint8_t* bytes, int byteCount)
{
	if (!bytes || byteCount < PICKUP_SNAPSHOT_BYTES || !gIsNetworkClient)
		return;
	const int count = gNumTerrainItems < PICKUP_SNAPSHOT_BYTES * 8 ? gNumTerrainItems : PICKUP_SNAPSHOT_BYTES * 8;
	for (int i = 0; i < count; i++)
		PangeaPickup_ReceiveState((uint16_t)i, (bytes[i >> 3] & (1u << (i & 7))) != 0);
}

uint32_t PangeaPickup_HashState(void)
{
	uint32_t hash = 2166136261u;
	for (int i = 0; i < gNumTerrainItems && i < PICKUP_SNAPSHOT_BYTES * 8; i++)
		hash = (hash ^ (uint32_t)(gPickupHidden[i] ? 1 : 0)) * 16777619u;
	return hash;
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
