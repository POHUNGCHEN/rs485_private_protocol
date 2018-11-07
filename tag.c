#include <string.h>
#include "tag.h"

Value *fieldbus_do_command(
        void            *this,
        uint8_t         *write_buffer, 
        int             write_size
        )
{
    Value   *value  = NULL;
    Tag     *tag    = (Tag *)this;
    
     // todo :

    return value;
}

int fieldbus_tag_check(void *this, Operate_t op_req)
{
    Tag         *tag = (Tag *)this;
    
    // todo :
    
    return 0;
}

int tag_setup(Tag *tag)
{
    tag->do_command = fieldbus_do_command;
    tag->check      = fieldbus_tag_check;

    return 0;
}