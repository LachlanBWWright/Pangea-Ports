#include "game.h"
#include <string.h>

#ifndef __EMSCRIPTEN__
void Nanosaur2Pickup_Reset(void) {}
Boolean Nanosaur2Pickup_CanCollect(ObjNode* pickup) { (void)pickup; return true; }
void Nanosaur2Pickup_Collected(ObjNode* pickup) { (void)pickup; }
void Nanosaur2Pickup_CollectedPermanent(ObjNode* pickup) { (void)pickup; }
void Nanosaur2Pickup_ApplyState(ObjNode* pickup) { (void)pickup; }
void Nanosaur2Pickup_ReceiveState(uint16_t itemIndex, Boolean hidden) { (void)itemIndex; (void)hidden; }
int Nanosaur2Pickup_WriteSnapshotState(uint8_t* bytes, int maxBytes) { (void)bytes; (void)maxBytes; return 0; }
void Nanosaur2Pickup_ReceiveSnapshotState(const uint8_t* bytes, int byteCount) { (void)bytes; (void)byteCount; }
uint32_t Nanosaur2Pickup_HashState(void) { return 0; }
void Nanosaur2Pickup_Update(void) {}
#else

#define PICKUP_SYNC_MAX 65536
#define PICKUP_SYNC_RESPAWN 10.0f
#define PICKUP_SYNC_SNAPSHOT_BYTES 512
static Boolean gPickupHidden[PICKUP_SYNC_MAX];
static float gPickupTimers[PICKUP_SYNC_MAX];

static int PickupIndex(const ObjNode* pickup)
{
	if (!pickup->TerrainItemPtr || !gMasterItemList)
		return -1;
	const ptrdiff_t index = pickup->TerrainItemPtr - gMasterItemList;
	return index >= 0 && index < gNumTerrainItems ? (int)index : -1;
}

static ObjNode* FindPickup(int index)
{
	for (ObjNode* node = gFirstNodePtr; node; node = node->NextNode)
		if (node->PICKUP_SYNC_MARKER && PickupIndex(node) == index)
			return node;
	return nil;
}

static void HidePickupObject(ObjNode* pickup)
{
	for (ObjNode* node = pickup; node; node = node->ChainNode)
	{
		node->CType = 0;
		node->StatusBits |= STATUS_BIT_HIDDEN;
		node->ColorFilter.a = 0.0f;
	}
}

void Nanosaur2Pickup_Reset(void)
{
	memset(gPickupHidden, 0, sizeof(gPickupHidden));
	memset(gPickupTimers, 0, sizeof(gPickupTimers));
}

Boolean Nanosaur2Pickup_CanCollect(ObjNode* pickup)
{
	(void) pickup;
	if (!PangeaNet_IsEnabled() || PangeaNet_IsHost())
		return true;
	return false;
}

void Nanosaur2Pickup_ApplyState(ObjNode* pickup)
{
	const int index = PickupIndex(pickup);
	if (index < 0)
		return;
	const Boolean hidden = gPickupHidden[index];
	if (hidden)
	{
		for (ObjNode* node = pickup; node; node = node->ChainNode)
		{
			node->CType = 0;
			node->StatusBits |= STATUS_BIT_HIDDEN;
			node->ColorFilter.a = 0.0f;
		}
	}
}

void Nanosaur2Pickup_Collected(ObjNode* pickup)
{
	if (!PangeaNet_IsEnabled() || !PangeaNet_IsHost())
		return;
	const int index = PickupIndex(pickup);
	if (index < 0 || gPickupHidden[index])
		return;
	gPickupHidden[index] = true;
	gPickupTimers[index] = PICKUP_SYNC_RESPAWN;
	PangeaNet_SendPickupState((uint16_t) index, true);
}

void Nanosaur2Pickup_CollectedPermanent(ObjNode* pickup)
{
	if (!PangeaNet_IsEnabled() || !PangeaNet_IsHost())
		return;
	const int index = PickupIndex(pickup);
	if (index < 0 || gPickupHidden[index])
		return;
	gPickupHidden[index] = true;
	gPickupTimers[index] = -1.0f;
	PangeaNet_SendPickupState((uint16_t) index, true);
}

void Nanosaur2Pickup_ReceiveState(uint16_t itemIndex, Boolean hidden)
{
	gPickupHidden[itemIndex] = hidden;
	gPickupTimers[itemIndex] = hidden ? PICKUP_SYNC_RESPAWN : 0.0f;
	ObjNode* pickup = FindPickup((int) itemIndex);
	if (!pickup)
		return;
	if (hidden)
	{
		if (gPickupTimers[itemIndex] < 0.0f)
			HidePickupObject(pickup);
		else
		{
			for (ObjNode* node = pickup; node; node = node->ChainNode)
			{
				node->CType = 0;
				node->ColorFilter.a = 0.0f;
			}
		}
	}
	else
	{
		pickup->CType = 0;
	}
}

int Nanosaur2Pickup_WriteSnapshotState(uint8_t* bytes, int maxBytes)
{
	if (!bytes || maxBytes < PICKUP_SYNC_SNAPSHOT_BYTES)
		return 0;
	memset(bytes, 0, PICKUP_SYNC_SNAPSHOT_BYTES);
	const int count = gNumTerrainItems < PICKUP_SYNC_SNAPSHOT_BYTES * 8 ? gNumTerrainItems : PICKUP_SYNC_SNAPSHOT_BYTES * 8;
	for (int i = 0; i < count; i++)
		if (gPickupHidden[i])
			bytes[i >> 3] |= (uint8_t)(1u << (i & 7));
	return PICKUP_SYNC_SNAPSHOT_BYTES;
}

void Nanosaur2Pickup_ReceiveSnapshotState(const uint8_t* bytes, int byteCount)
{
	if (!bytes || byteCount < PICKUP_SYNC_SNAPSHOT_BYTES || !PangeaNet_IsEnabled() || PangeaNet_IsHost())
		return;
	const int count = gNumTerrainItems < PICKUP_SYNC_SNAPSHOT_BYTES * 8 ? gNumTerrainItems : PICKUP_SYNC_SNAPSHOT_BYTES * 8;
	for (int i = 0; i < count; i++)
		Nanosaur2Pickup_ReceiveState((uint16_t)i, (bytes[i >> 3] & (1u << (i & 7))) != 0);
}

uint32_t Nanosaur2Pickup_HashState(void)
{
	uint32_t hash = 2166136261u;
	for (int i = 0; i < gNumTerrainItems && i < PICKUP_SYNC_SNAPSHOT_BYTES * 8; i++)
		hash = (hash ^ (uint32_t)(gPickupHidden[i] ? 1 : 0)) * 16777619u;
	return hash;
}

void Nanosaur2Pickup_Update(void)
{
	if (!PangeaNet_IsEnabled() || !PangeaNet_IsHost())
		return;
	for (int index = 0; index < gNumTerrainItems && index < PICKUP_SYNC_MAX; index++)
	{
		if (!gPickupHidden[index])
			continue;
		if (gPickupTimers[index] < 0.0f)
			continue;
		gPickupTimers[index] -= gFramesPerSecondFrac;
		if (gPickupTimers[index] > 0.0f)
			continue;
		gPickupHidden[index] = false;
		ObjNode* pickup = FindPickup(index);
		if (pickup)
		{
			pickup->CType = 0;
		}
		PangeaNet_SendPickupState((uint16_t) index, false);
	}
}
#endif
