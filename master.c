#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> 
#include <unistd.h>
#include "mxfb_log.h"  
#include "mxzmq.h"
#include "equipment.h"
#include "tag.h"
#include "mxfbi.h"
#include <jansson.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "mxfb_util.h"

static volatile int EXIT_SIGINT = 0;
static volatile int debug_mode = 0;
static volatile int front_mode = 0;
static volatile int time_mode = 0;

static void sigint_handler(int sig)
{
    EXIT_SIGINT = 1;
}

void usage()
{
    printf("\n%s : version %s\n", MASTER_NAME, VERSION);
    printf("Allowed options:\n");
    printf("\t-d [ --foreground ]   enable debug mode.\n");
    printf("\t-h                    Print this manual.\n");
}

int arg_setup(int argc, char **argv)
{
    char c;
    char optstring[] = "fhdt:";
    
    while ((c = getopt(argc, argv, optstring)) != 0xFF)
    {
        switch (c)
        {
        case 'h':
            usage();
            exit(0);
        case 'f':
            front_mode = 1;
            break;
        case 'd':
            debug_mode = 1; 
            break;
        case 't':
            siginterrupt(SIGALRM, 1);
            signal(SIGALRM, sigint_handler);
            time_mode = atoi(optarg);
            break;
        default:
            if (((int)c) < 0) return 0;
            break;
        }
    }
}

#pragma pack (1)
typedef struct
{
    Equipment *all_equipment_list;
    int equipment_count;
} mxfbi_userdata;
#pragma pack ()

int read_tag_callback (char *equipment_name, char *tag_name, value_t* read_value, value_type_t* read_value_type, void *usr_data)
{
    if (!read_value || !read_value_type)
    {
        MXLOG_DBUG("read_tag_callback() invalid argument");
        return -1;
    }

    MXLOG_DBUG("equ[%s]:tag[%s] tag_callback in.", equipment_name, tag_name);
    int i;
    Tag *tag = NULL;
    Value *value;
    mxfbi_userdata *equ_list = (mxfbi_userdata*)usr_data;

    for (i=0; i<equ_list->equipment_count; i++)
    {
        Equipment *equ = &(equ_list->all_equipment_list[i]);
        
        if (strcmp(equipment_name, equ->name) != 0) continue;

        if ((tag = equ->get_tag(equ, tag_name)) == NULL)
        {
            MXLOG_EROR("REP_TAG_NOTFOUND");
            return -1;
        }

        if (tag->check(tag, MXFB_READ) != 0)
        {
            MXLOG_EROR("REP_TAG_OP_MISMATCH");
            return -1;
        }

        value = equ->task_insert(equ, tag, NULL, 0);
        if (value == NULL) 
        {
            MXLOG_EROR("REP_TASK_INSERT_ERROR");
            return -1;
        }

        if (mx_value_v2_transfer(value->payload, value->type_t, value->size, read_value, read_value_type) != 0)
        {
            MXLOG_EROR("mx_value_v2_transfer failed");
            return -1;
        }

        return 0;
    }
    MXLOG_EROR("REP_EQU_NOTFOUND");
    return -1;
}

int write_tag_callback (char *equipment_name, char *tag_name, value_t w_value_t, value_type_t w_value_type, void *usr_data)
{
    MXLOG_DBUG("equ[%s]:tag[%s] tag_callback in.", equipment_name, tag_name);
    int i;
    Tag *tag = NULL;
    Value *value;
    uint8_t *w_value = NULL;
    mxfbi_userdata *equ_list = (mxfbi_userdata*)usr_data;

    for (i=0; i<equ_list->equipment_count; i++)
    {
        Equipment *equ = &(equ_list->all_equipment_list[i]);

        if (strcmp(equipment_name, equ->name) != 0) continue;

        if ((tag = equ->get_tag(equ, tag_name)) == NULL)
        {
            MXLOG_EROR("REP_TAG_NOTFOUND");
            return -1;
        }

        if (tag->check(tag, MXFB_WRITE) != 0)
        {
            MXLOG_EROR("REP_TAG_OP_MISMATCH");
            return -1;
        }
        uint8_t *write_buffer = NULL;
        int write_size = 0;

        switch(w_value_type)
        {
            case TAG_VALUE_TYPE_INT: 
                write_size = sizeof(value_t);
                write_buffer = (uint8_t*)calloc(1, sizeof(value_t));
                strncpy(write_buffer, (char*)&w_value_t.i, write_size);
            break;
            case TAG_VALUE_TYPE_UINT:
                write_size = sizeof(value_t);
                write_buffer = (uint8_t*)calloc(1, sizeof(value_t));
                strncpy(write_buffer, (char*)&w_value_t.u, write_size);
            break;
            case TAG_VALUE_TYPE_DOUBLE:
                write_size = sizeof(value_t);
                write_buffer = (uint8_t*)calloc(1, sizeof(value_t));
                strncpy(write_buffer, (char*)&w_value_t.d, write_size);
            break;
            case TAG_VALUE_TYPE_FLOAT:
                write_size = sizeof(value_t);
                write_buffer = (uint8_t*)calloc(1, sizeof(value_t));
                strncpy(write_buffer, (char*)&w_value_t.f, write_size);
            break;
            case TAG_VALUE_TYPE_STRING:
                write_size = strlen(w_value_t.s);
                write_buffer = (uint8_t*)calloc(1, strlen(w_value_t.s));
                strncpy(write_buffer, (char*)&w_value_t.s, write_size);
            break;
            case TAG_VALUE_TYPE_BYTEARRAY:
                write_size = w_value_t.l;
                write_buffer = (uint8_t*) calloc(1, w_value_t.l);
                strncpy(write_buffer, (char*)&w_value_t.b, write_size);
            break;
            default:
                MXLOG_EROR("write_tag_callback() invalid value type");
                return -1;
            break;
        }

        value = equ->task_insert(equ, tag, write_buffer, write_size);

        if (write_buffer != NULL)
            free(write_buffer);

        if (value == NULL)
        {
            MXLOG_EROR("REP_TASK_INSERT_ERROR");
            return -1;
        }

        return 0;
    }
    MXLOG_EROR("REP_EQU_NOTFOUND");
    return -1;
}

char* status_callback (void *obj)
{    
    if (NULL == obj) {
        return NULL;
    }

    mxfbi_userdata* mxfbi_userdata_obj = (mxfbi_userdata*)obj;
    
    int i = 0;
    long long ts = 0;
    char ts_now[0x30] = {0};
    current_timestamp(&(ts));
    timestamp_to_date_str(ts, ts_now, sizeof(ts_now));

    /*Creating a json object*/
    json_t *j_status_obj = json_object();
    json_t *j_equ_array = json_array();

    for (i = 0; i < mxfbi_userdata_obj->equipment_count; i++)
    {
        Equipment *equ  = &mxfbi_userdata_obj->all_equipment_list[i];
        json_t *j_equ_item_obj = json_object();

        int j = 0, success_count = 0;
        for (j = 0; j < equ->tag_count; j++) 
        {
            Tag *tag = equ->tags + j;
            if (tag && tag->polling_status == 1) {
                success_count++;
            }
        }

        json_t *j_tag_array = json_array();
        for (j = 0; j < equ->tag_count; j++)
        {
            Tag *tag = equ->tags + j;
            json_t *j_tag_item_obj = json_object();
            json_object_set(j_tag_item_obj,"name", json_string(tag->id));
            json_object_set(j_tag_item_obj,"status", json_integer(tag->polling_status));
            json_object_set(j_tag_item_obj,"lastPollingAt", json_string(tag->last_polling_ts));
            json_object_set(j_tag_item_obj,"lastSuccessAt", json_string(tag->last_success_ts));

            // Add json object item into json array
            json_array_append(j_tag_array, j_tag_item_obj);

        }

        json_object_set(j_equ_item_obj, "name", json_string(equ->name));
        json_object_set(j_equ_item_obj, "totalTags", json_integer(equ->tag_count));
        json_object_set(j_equ_item_obj, "successTags", json_integer(success_count));
        json_object_set(j_equ_item_obj, "tagList", j_tag_array);

        json_array_append(j_equ_array, j_equ_item_obj);
    }   
    
    json_object_set(j_status_obj, "at", json_string(ts_now));
    json_object_set(j_status_obj, "deviceList", j_equ_array);
    //MXLOG_DBUG("The json object created: %s\n", json_dumps(j_status_obj));

    return json_dumps(j_status_obj, 0);
}

int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if ( (he = gethostbyname( hostname ) ) == NULL)
    {
        // get the host info
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]));
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{    
    Equipment           *all_equipment_list;
    mxfbi_userdata      mxfbi_arg;
    mxfbi_t             *mxfbi;
    tag                 *tag_ctx = NULL;
    int                 equipment_count = 0, api_timeout = 1;
    int                 i, rc, fd;

    // init arg.
    arg_setup(argc, argv);
    
    if (front_mode == 0)
        daemon(0, 0);
    // init log function.
    mxlog_init(MASTER_NAME, debug_mode);

    // install signal handler
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    signal(SIGKILL, sigint_handler);

    // start tag interface(Mqtt).
    tag_ctx = mxtag_new();
    if (tag_ctx == NULL)
    {
        MXLOG_EROR("tag interface init fail.");
        return -1;
    }
    
    // load config and init equipment list.
    equipment_count = config_parse(CONFIG_PATH, &all_equipment_list);
    if (equipment_count <= 0) 
    {
        MXLOG_EROR("No equipment is written in config.");
        return -1;
    }
    MXLOG_INFO("equipment_count : %d", equipment_count);

    // prepare arg to mxfbi callback.
    mxfbi_arg.all_equipment_list = all_equipment_list;
    mxfbi_arg.equipment_count = equipment_count;
    
    // init mxfbi.
    mxfbi = mxfbi_new();
    if (mxfbi == NULL)
    {
        MXLOG_EROR("%s master direct interface bind fail.");
        return -1;
    }

    if ((fd = set_master_pid(MASTER_PID_PATH)) <= 0)
        return -1;

    mxfbi_read_tag_callback_set(mxfbi, (void*)&mxfbi_arg, read_tag_callback);
    mxfbi_write_tag_callback_set(mxfbi, (void*)&mxfbi_arg, write_tag_callback);
    mxfbi_status_callback_set(mxfbi, (void*)&mxfbi_arg, status_callback);
    
    // setup and start all equipment
    for (i=0; i<equipment_count; i++)
    {
        Equipment *equ  = &all_equipment_list[i];
        equ->tag_ctx = tag_ctx;
        if (equ->init(equ) == 0)
            equ->start(equ);
    }

    if (time_mode) alarm(time_mode);

    // main process start monitor signal or recv client msg here.
    mxfbi_start(mxfbi);
    while(!EXIT_SIGINT)
    {
        sleep(1);
    }

    // stop all equipment scheduler
    for (i=0; i<equipment_count; i++)
    {
        Equipment *equ = &all_equipment_list[i];
        equ->stop(equ);
    }

    // enable log when test time mode(show publish count).
    if (time_mode) 
    {
        long long process_tag_count = 0;
        mxlog_debug_on();
        
        for (i=0; i<equipment_count; i++)
        {
            Equipment *equ = &all_equipment_list[i];
            process_tag_count += equ->polling_count;
        }
        printf("total reading count : %lld\n", process_tag_count);
    }
    // stop ipc resource(Mqtt).
    if (tag_ctx != NULL) {
        mxtag_delete(tag_ctx);
    }

    mxfbi_delete(mxfbi);
    MXLOG_INFO("%s main process end.", MASTER_NAME);
    mxlog_close();
    close(fd);

    char cmd[256] = {0};
    sprintf(cmd, "rm -f %s", MASTER_PID_PATH);
    system(cmd);

    return 0;
}