#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <jansson.h>
#include "mxzmq.h"
#include "equipment.h"
#include "tag.h"
#include "mxfb_util.h"
#include "mxfb_log.h"

int free_tag(Tag *tag)
{
    if (tag->id != NULL)free(tag->id);
    if (tag->op != NULL)free(tag->op);
    if (tag->type != NULL)free(tag->type);
    if (tag->unit != NULL)free(tag->unit);
    if (tag->byte_order != NULL)free(tag->byte_order);
    free(tag);
    
    return 0;
}

int make_tag (
    Tag *tag, 
    json_t *tag_setting
    )
{
    const char *str;
    json_t *j_obj;
    
    // don't modify this var, this use for framework identify tag context. 
    tag->version = FW_TAG_VERSION;

    // id
    if ((str = get_string_form_json_object(tag_setting, "id")) != NULL)
    {
        tag->id = (char *)malloc(strlen(str)+1);
        strcpy(tag->id, str);
        MXLOG_DBUG("tag id : %s", tag->id);
    } else return -1;
    
    // op
    if ((str = get_string_form_json_object(tag_setting, "op")) != NULL)
    {
        tag->op = (char *)malloc(strlen(str)+1);
        strcpy(tag->op, str);
        MXLOG_DBUG("tag op : %s", tag->op);
    } else return -1;
    
    // type
    if ((str = get_string_form_json_object(tag_setting, "type")) != NULL)
    {
        tag->type = (char *)malloc(strlen(str)+1);
        strcpy(tag->type, str);
        MXLOG_DBUG("tag type : %s", tag->type);
    } else return -1;
    
    // requestTimeoutMs
    if ((j_obj = json_object_get(tag_setting, "requestTimeoutMs")) != NULL)
    {
        tag->request_timeout_ms  = json_integer_value(j_obj);
        MXLOG_DBUG("tag requestTimeoutMs : %d", tag->request_timeout_ms);
    }
    else return -1;
    
    // pollingPeriodMs
    if ((j_obj = json_object_get(tag_setting, "pollingPeriodMs")) != NULL)
    {
        tag->polling_period_ms  = json_integer_value(j_obj);
        MXLOG_DBUG("tag pollingPeriodMs : %d", tag->polling_period_ms);
    }
    else return -1;
    
    // unit
    if ((str = get_string_form_json_object(tag_setting, "unit")) != NULL)
    {
        tag->unit = (char *)malloc(strlen(str)+1);
        strcpy(tag->unit, str);
        MXLOG_DBUG("tag unit : %s", tag->unit);
    }
    else tag->unit = (char *)calloc(1, 2);
    
    // byteOrder
    if ((str = get_string_form_json_object(tag_setting, "byteOrder")) != NULL)
    {
        tag->byte_order = (char *)malloc(strlen(str)+1);
        strcpy(tag->byte_order, str);
        MXLOG_DBUG("tag byteOrder : %s", tag->byte_order);
    }
    else tag->byte_order = (char *)calloc(1, 2);
    
    // interframeDelayMs
    if ((j_obj = json_object_get(tag_setting, "interframeDelayMs")) != NULL)
    {
        tag->interframe_delay_ms  = json_integer_value(j_obj);
        MXLOG_DBUG("tag interframeDelayMs : %d", tag->interframe_delay_ms);
    }
    
    // address
    if ((j_obj = json_object_get(tag_setting, "address")) != NULL)
    {
        tag->address  = json_integer_value(j_obj);
        MXLOG_DBUG("tag address : %d", tag->address);
    }
    else return -1;
    
    // cid1
    if ((j_obj = json_object_get(tag_setting, "cid1")) != NULL)
    {
        tag->cid1  = json_integer_value(j_obj);
        MXLOG_DBUG("tag cid1 : %d", tag->cid1);
    }
    else return -1;
    
    // cid2
    if ((j_obj = json_object_get(tag_setting, "cid2")) != NULL)
    {
        tag->cid2  = json_integer_value(j_obj);
        MXLOG_DBUG("tag cid2 : %d", tag->cid2);
    }
    else return -1;
    
    // length
    if ((j_obj = json_object_get(tag_setting, "length")) != NULL)
    {
        tag->length  = json_integer_value(j_obj);
        MXLOG_DBUG("tag length : %d", tag->length);
    }
    else return -1;
    
    // info
    if ((j_obj = json_object_get(tag_setting, "info")) != NULL)
    {
        tag->info  = json_integer_value(j_obj);
        MXLOG_DBUG("tag info : %d", tag->info);
    }
    else return -1;
    
    
    tag_setup(tag);
    return 0;
}

Tag *make_tag_from_string(
    char *tag_json
    )
{
    json_t *tag_obj;
    json_error_t error;
    Tag *tag = (Tag*)calloc(1, sizeof(Tag));
    
    tag_obj = json_loads(tag_json, 0, &error);
    if (!tag_obj)
    {
        free(tag);
        return NULL;
    }
    if (make_tag(tag, tag_obj) < 0)
    {
        free(tag);
        return NULL;
    }
    return tag;
}

int config_parse(char *path, Equipment **equ_list_in)
{
    int i, j, t, device_count = 0;
    Equipment *equ_list;
    json_t *cfg, *device_list;
    json_error_t error;

    if (!isDirectory(path))
    {
        MXLOG_EROR("Path %s is not a folder!!", path);
        return -1;
    }

    cfg = json_load_file(CONFIG_FILE, 0, &error);
    if (!cfg) 
    {
        MXLOG_EROR("File[%s] parse failed.!!", CONFIG_FILE);
        return -1;
    }
    
    device_list = json_object_get(cfg, "deviceList");
    if(device_list == NULL)
    {
        MXLOG_EROR("File[%s] no deviceList!!", CONFIG_FILE);
        return -1;
    }
    
    if ((device_count = json_array_size(device_list)) <= 0)
    {
        MXLOG_EROR("File[%s] device_count is invalid[%d]!!", CONFIG_FILE, device_count);
        return -1;
    }
    MXLOG_DBUG("device_count = %d\n", device_count);
    
    equ_list = (Equipment *)calloc(1, sizeof(Equipment)*device_count);
    for(i = 0; i < device_count; i++)
    {
        Equipment *equ;
        int tag_count;
        char template_path[1024] = {0};
        const char *str;
        json_t *device, *t_cfg, *tags, *tag_obj, *j_obj;
        
        equ     = &(equ_list[i]);
        device  = json_array_get(device_list, i);
        
        // name
        if ((str = get_string_form_json_object(device, "name")) != NULL)
        {
            equ->name = (char *)malloc(strlen(str)+1);
            strcpy(equ->name, str);
            MXLOG_DBUG("equipment name : %s", equ->name);
        } 
        else 
        { 
            MXLOG_EROR("File[%s] get name fail.", CONFIG_FILE); 
            continue; 
        }

        // interface
        if ((str = get_string_form_json_object(device, "interface")) != NULL)
        {
            equ->interface = (char *)malloc(strlen(str)+1);
            strcpy(equ->interface, str);
            MXLOG_DBUG("equipment interface : %s", equ->interface);
        } 
        else 
        { 
            MXLOG_EROR("File[%s] get interface fail.", CONFIG_FILE); 
            continue; 
        }

        // templateName
        if ((str = get_string_form_json_object(device, "templateName")) != NULL)
        {
            equ->template_name = (char *)malloc(strlen(str)+1);
            strcpy(equ->template_name, str);
            MXLOG_DBUG("equipment templateName : %s", equ->template_name);
        } 
        else 
        { 
            MXLOG_EROR("File[%s] get templateName fail.", CONFIG_FILE); 
            continue; 
        }

        
        sprintf(template_path, "%stemplate.d/%s", CONFIG_PATH, equ->template_name);
        t_cfg = json_load_file(template_path, 0, &error);
        if (!t_cfg) 
        {
            MXLOG_EROR("Equipment[%s] load template[%s] failed.!!", equ->name, template_path);
            return -1;
        }
        
        // check have tags.
        if ((tags = json_object_get(t_cfg, "tagList")) == NULL)
        {
            MXLOG_EROR("Equipment[%s] template[%s] no tags.!!", equ->name, template_path);
            continue;
        }
        
        tag_count = json_array_size(tags);
        equ->tag_count = tag_count;
        equ->tags = (Tag *)malloc(sizeof(Tag)*tag_count);
        MXLOG_EROR("Equipment[%s] template[%s] tag count : %d.!!", equ->name, template_path, equ->tag_count);
        
        // create tag_groups.
        int *polling_ms_arr = (int*)calloc(1, sizeof(int)*tag_count);
        int *group_tag_count_arr = (int*)calloc(1, sizeof(int)*tag_count);
        int tag_group_count = 0;
        for (t = 0; t < tag_count; t++) 
        {
            int polling_period_ms = 0;
            
            tag_obj = json_array_get(tags, t);
            polling_period_ms  = get_int_form_json_object(tag_obj, "pollingPeriodMs");
            for (j=0; j<tag_count; j++) 
            {
                if (polling_period_ms == polling_ms_arr[j])
                {
                    group_tag_count_arr[j]++;
                    break;
                }
                if (polling_ms_arr[j] == 0)
                {
                    MXLOG_DBUG("equ:%s, tag group[%d] pollingPeriodMs = %d\n", 
                                    equ->name, 
                                    tag_group_count, 
                                    polling_period_ms
                                    );
                    polling_ms_arr[j] = polling_period_ms;
                    tag_group_count++;
                    group_tag_count_arr[j]++;
                    break;
                }
            }
        }
        MXLOG_DBUG("equ:%s, tag group count = %d\n", equ->name, tag_group_count);
        equ->t_groups = (Tag_group *)calloc(1, sizeof(Tag_group) * (tag_group_count+1));
        
        //init each tag groups.
        for (j=0; j<tag_group_count; j++) 
        {
            Tag_group *t_groups = &(equ->t_groups[j]);
            t_groups->equipment = equ;
            t_groups->scheduler.polling_period_ms = polling_ms_arr[j];
            t_groups->tag_count = 0;
            t_groups->tags = (Tag**)malloc(sizeof(Tag*) * group_tag_count_arr[j]);
            memset(t_groups->tags, 0, (sizeof(Tag*) * group_tag_count_arr[j]));
        }
        
        // set empty group to end.
        equ->t_groups[tag_group_count].tag_count = 0;

        // init each tag.
        for (t = 0; t < tag_count; t++) 
        {
            Tag *tag = (equ->tags+t);
            tag_obj = json_array_get(tags, t);

            if (make_tag(tag, tag_obj) != 0)
            {
                MXLOG_EROR("File[%s] tag parse failed.!!", template_path);
                return -1;
            }

            for (j = 0; j<tag_group_count; j++) 
            {
                if (equ->t_groups[j].scheduler.polling_period_ms == 
                    tag->polling_period_ms)
                {
                    int tag_idx = equ->t_groups[j].tag_count;
                    equ->t_groups[j].tags[tag_idx] = tag;
                    equ->t_groups[j].tag_count++;
                    MXLOG_DBUG("equ:%s, group[%d] add tag[%d/%d:%s:%X].\n", 
                                    equ->name, 
                                    equ->t_groups[j].scheduler.polling_period_ms,
                                    tag_idx, group_tag_count_arr[j], tag->id, tag);
                }
            }
        }

        free(polling_ms_arr);
        free(group_tag_count_arr);

        mxequipment_init(equ, equ_init_hook);
    }

    *equ_list_in = equ_list;
    return device_count;
}
