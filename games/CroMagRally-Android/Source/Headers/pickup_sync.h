#pragma once

void PangeaPickup_Reset(void);
void PangeaPickup_Update(void);
Boolean PangeaPickup_CanCollect(ObjNode* node);
void PangeaPickup_Collect(ObjNode* node);
Boolean PangeaPickup_ApplyObject(ObjNode* node);
void PangeaPickup_ReceiveState(uint16_t itemIndex, Boolean hidden);
void PangeaNet_SendPickupState(uint16_t itemIndex, Boolean hidden);
