#ifndef MX_EQUIPMENT_H 
#define MX_EQUIPMENT_H

#include "mxzmq.h"
#include "tag.h"
#include "mxfb_equipment.h"
#include "mxfb_scheduler.h"

#pragma pack (1)
typedef struct
{
//--------------------------------------------------
// ThingsPro Equipment comman item
//--------------------------------------------------
    COMMON_EQU_ITEM
//--------------------------------------------------
// ThingsPro Equipment protocol specific item
//--------------------------------------------------
} Equipment;
#pragma pack ()

// user need implement this hook function.
extern int equ_init_hook(Equipment *equ);
#endif
