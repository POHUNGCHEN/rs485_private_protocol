#ifndef MX_TAG_H 
#define MX_TAG_H

#include "mxfb.h"
#include "mxfb_tag.h"

#pragma pack (1)
typedef struct
{   
//--------------------------------------------------
// ThingsPro IO tag comman item
//--------------------------------------------------
    COMMON_TAG_ITEM
//--------------------------------------------------
// ThingsPro IO tag protocol specific item
//--------------------------------------------------
    int                 address;
    int                 cid1;
    int                 cid2;
    float               length;
    float               info;
} Tag;
#pragma pack ()

extern int tag_setup(
            Tag *tag
            );
#endif
