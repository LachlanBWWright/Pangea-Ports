#pragma once

Boolean Nanosaur2Script_CarryEgg(short player, ObjNode* egg);
Boolean Nanosaur2Script_ResetCarriedEgg(short player);
Boolean Nanosaur2Script_DeliverEgg(short player, ObjNode* destination);
Boolean Nanosaur2Script_HandoverEgg(short player, short receiver);
ObjNode* Nanosaur2Script_GetEggNest(ObjNode* egg);
void Nanosaur2Script_AlignCarriedEgg(ObjNode* egg);
