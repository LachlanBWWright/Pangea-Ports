//
// pickups.h
//

#define	PickUpCollisionRadius	SpecialF[5]

extern	Boolean AddEgg(TerrainItemEntryType *itemPtr, long  x, long z);
extern	void TryToDoPickUp(ObjNode *theNode, Byte limbNum);
extern	void DropItem(ObjNode *theNode);
extern	void GetAllEggsCheat(void);
Boolean CarryEgg(ObjNode *holder, ObjNode *egg);
Boolean DeliverEgg(ObjNode *egg, ObjNode *portal);
void ReleasePickupRelationships(ObjNode *object);
Boolean UpdateCarriedEgg(ObjNode* egg);

