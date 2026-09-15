#include "game.h"
#include "pickup_sync.h"
#include <assert.h>

Boolean gIsNetworkHost;
Boolean gIsNetworkClient;
short gNumTerrainItems = 2;
TerrainItemEntryType** gMasterItemList;
float gFramesPerSecondFrac;
static int eventCount;
static Boolean lastHidden;

void PangeaNet_SendPickupState(uint16_t itemIndex, Boolean hidden)
{
	assert(itemIndex == 1);
	eventCount++;
	lastHidden = hidden;
}

int main(void)
{
	TerrainItemEntryType items[2] = {0};
	TerrainItemEntryType* itemList = items;
	gMasterItemList = &itemList;
	ObjNode shadow = {0};
	ObjNode pickup = {0};
	pickup.TerrainItemPtr = &items[1];
	pickup.ShadowNode = &shadow;
	gIsNetworkHost = true;
	PangeaPickup_Reset();
	assert(PangeaPickup_CanCollect(&pickup));
	PangeaPickup_Collect(&pickup);
	PangeaPickup_Collect(&pickup);
	assert(eventCount == 1 && lastHidden);
	assert(!PangeaPickup_CanCollect(&pickup));
	assert(PangeaPickup_ApplyObject(&pickup));
	assert(pickup.CType == 0 && (shadow.StatusBits & STATUS_BIT_HIDDEN));

	// Cooldown progresses even when the pickup object has streamed out.
	gFramesPerSecondFrac = 4.0f;
	PangeaPickup_Update();
	assert(eventCount == 1);
	gFramesPerSecondFrac = 1.0f;
	PangeaPickup_Update();
	assert(eventCount == 2 && !lastHidden);
	assert(PangeaPickup_CanCollect(&pickup));

	gIsNetworkHost = false;
	gIsNetworkClient = true;
	PangeaPickup_Reset();
	PangeaPickup_ReceiveState(1, true);
	PangeaPickup_ReceiveState(UINT16_MAX, true);
	ObjNode streamedPickup = {0};
	streamedPickup.TerrainItemPtr = &items[1];
	assert(PangeaPickup_ApplyObject(&streamedPickup));
	assert(streamedPickup.StatusBits & STATUS_BIT_HIDDEN);
	assert(!PangeaPickup_CanCollect(&streamedPickup));
	gFramesPerSecondFrac = 10.0f;
	PangeaPickup_Update();
	PangeaPickup_ApplyObject(&streamedPickup);
	assert(streamedPickup.StatusBits & STATUS_BIT_HIDDEN);
	PangeaPickup_ReceiveState(1, false);
	PangeaPickup_ApplyObject(&streamedPickup);
	assert(!(streamedPickup.StatusBits & STATUS_BIT_HIDDEN));
	assert(streamedPickup.CType == CTYPE_TRIGGER);
	gIsNetworkClient = false;
	assert(PangeaPickup_CanCollect(&streamedPickup));
	assert(!PangeaPickup_ApplyObject(&streamedPickup));
	return 0;
}
