#pragma once

void PangeaPickup_Reset(void);
void PangeaPickup_Update(void);
Boolean PangeaPickup_CanCollect(ObjNode* node);
void PangeaPickup_Collect(ObjNode* node);
void PangeaPickup_CollectPermanent(ObjNode* node);
Boolean PangeaPickup_ApplyObject(ObjNode* node);
void PangeaPickup_ReceiveState(uint16_t itemIndex, Boolean hidden);
int PangeaPickup_WriteSnapshotState(uint8_t* bytes, int maxBytes);
void PangeaPickup_ReceiveSnapshotState(const uint8_t* bytes, int byteCount);
uint32_t PangeaPickup_HashState(void);
void PangeaNet_SendPickupState(uint16_t itemIndex, Boolean hidden);
