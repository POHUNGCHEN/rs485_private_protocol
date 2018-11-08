#include "equipment.h"
#include "mxfb_rule.h"
#include "trina_rs485.h"

//char *_cl_port;

int timestamp_to_date_str(long long poll_last_ts, char *last_tstr, int last_tstr_len)
{    
    struct tm *t = NULL;
    time_t sec = (time_t)(poll_last_ts/1000);
    t = gmtime(&sec);    
    strftime(last_tstr, last_tstr_len, "%Y-%m-%dT%H:%M:%SZ", t);
    return 0;
}

Value *tag_polling_hook (void *this, Tag *tag)
{
    Equipment   *equ    = (Equipment *)this;
    Value       *value  = NULL;
    
    long long current_ts;
    current_timestamp(&current_ts);
    timestamp_to_date_str(current_ts, tag->last_polling_ts, 0x30);
    tag->polling_status = 0;
    
    // do polling command
    value = tag->do_command(tag, NULL, 0);

    if (value != NULL) // polling succeed
    {
        value = mx_rule_handler(tag, value);
        long long current_ts;
        current_timestamp(&current_ts);
        timestamp_to_date_str(current_ts, tag->last_success_ts, 0x30);
        tag->polling_status = 1;
    }

    return value;
}

int equ_restart_hook (void *this)
{
    Equipment *equ = (Equipment *)this;
    // todo :
    /*
    MXLOG_INFO("RS485 serial port reconnecting...\n");
    if(mx_rs485_reconnect())
    {
        MXLOG_EROR("RS485 serial port connection fail.\n");
        return -1;
    }
    MXLOG_INFO("RS485 serial port reconnect success.");
    */
    // todo :
    return 0;
}

int equ_is_warning_hook (void *this)
{
    Equipment *equ = (Equipment *)this;
    // todo :
    return 0;
}

int equ_stop_hook (void *this)
{
    Equipment *equ = (Equipment *)this;
    // todo :
    //mx_rs485_close();
    // todo :
    return 0;
}

int equ_start_hook (void *this)
{
    Equipment *equ = (Equipment *)this;
    // todo :
    // ******** open RS485 port
    
    if(mx_rs485_new(_cl_port) < 0)
    {
        MXLOG_EROR("RS485 port open fail.\n");
        return -1;
    }
    
    // todo :
    return 0;
}

Value *equ_task_insert_hook (
        void   *this, 
        Tag         *tag, 
        uint8_t     *buffer, 
        int         buffer_size
        )
{
    Equipment   *equ    = (Equipment *)this;
    Value       *value  = NULL;
    
    // equ->lock(equ);
    // todo :
    // equ->unlock(equ);
    return value;
}

int equ_init_hook(Equipment *equ)
{
    // setup callback.
    mxequipment_start_hook_set(equ, equ_start_hook);
    mxequipment_stop_hook_set(equ, equ_stop_hook);
    mxequipment_is_warning_hook_set(equ, equ_is_warning_hook);
    mxequipment_restart_hook_set(equ, equ_restart_hook);
    mxequipment_tag_polling_hook_set(equ, tag_polling_hook);
    mxequipment_task_insert_hook_set(equ, equ_task_insert_hook);

    int i, equ_tag_count;
    Tag *tags = equ->tags;
    equ_tag_count = equ->tag_count;
    
    for (i = 0; i < equ_tag_count; i++)
    {
        Tag *tag        = tags+i;
        tag->tag_ctx    = equ->tag_ctx;
        tag->last_polling_ts = (char*)malloc(0x30);
        tag->last_success_ts = (char*)malloc(0x30);
        memset(tag->last_polling_ts, 0, 0x30);
        memset(tag->last_success_ts, 0, 0x30);
        tag->polling_status = 0;
        tag->offset = 0.0f;
        tag->slope = 0.0f;
        tag->interframe_delay_ms = 0;
    }

    return 0;
}