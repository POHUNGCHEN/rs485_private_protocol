#include <stdlib.h> 
#include "Trinaframe.h"

#define _TF_FN

#define TF_CKSUM_TYPE TF_CKSUM_CUSTOM16
#define TF_CKSUM_CUSTOM16 2  // Custom 16-bit checksum

// Helper macros
#define TF_MIN(a, b) ((a)<(b)?(a):(b))
#define TF_TRY(func) do { if(!(func)) return false; } while (0)

// Type-dependent masks for bit manipulation in the ID field
#define TF_ID_MASK (TF_ID)(((TF_ID)1 << (sizeof(TF_ID)*8 - 1)) - 1)
#define TF_ID_PEERBIT (TF_ID)((TF_ID)1 << ((sizeof(TF_ID)*8) - 1))

#define CKSUM_RESET(cksum)     do { (cksum) = TF_CksumStart(); } while (0)
#define CKSUM_ADD(cksum, byte) do { (cksum) = TF_CksumAdd((cksum), (byte)); } while (0)
#define CKSUM_FINALIZE(cksum)  do { (cksum) = TF_CksumEnd((cksum)); } while (0)

/** Claim the TX interface before composing and sending a frame */
static bool TF_ClaimTx(TinyFrame *tf) {
    if (tf->soft_lock) {
        TF_Error("TF already locked for tx!");
        return false;
    }
    tf->soft_lock = true;
    return true;
}

/** Free the TX interface after composing and sending a frame */
static void TF_ReleaseTx(TinyFrame *tf)
{
    tf->soft_lock = false;
}

//region Init
/** Init with a user-allocated buffer */
bool _TF_FN TF_InitStatic(TinyFrame *tf, TF_Peer peer_bit)
{
    if (tf == NULL) {
        TF_Error("TF_InitStatic() failed, tf is null.");
        return false;
    }

    // Zero it out, keeping user config
    uint32_t usertag = tf->usertag;
    void * userdata = tf->userdata;

    memset(tf, 0, sizeof(struct TinyFrame_));

    tf->usertag = usertag;
    tf->userdata = userdata;

    tf->peer_bit = peer_bit;
    return true;
}

/** Init with malloc */
TinyFrame * _TF_FN TF_Init(TF_Peer peer_bit)
{
    TinyFrame *tf = malloc(sizeof(TinyFrame));
    if (!tf) {
        TF_Error("TF_Init() failed, out of memory.");
        return NULL;
    }

    TF_InitStatic(tf, peer_bit);
    return tf;
}

/** Release the struct */
void TF_DeInit(TinyFrame *tf)
{
    if (tf == NULL) return;
    free(tf);
}

//endregion Init


//region Listeners

/** Reset ID listener's timeout to the original value */
static inline void _TF_FN renew_id_listener(struct TF_IdListener_ *lst)
{
    lst->timeout = lst->timeout_max;
}

/** Notify callback about ID listener's demise & let it free any resources in userdata */
static void _TF_FN cleanup_id_listener(TinyFrame *tf, TF_COUNT i, struct TF_IdListener_ *lst)
{
    TF_Msg msg;
    if (lst->fn == NULL) return;

    // Make user clean up their data - only if not NULL
    if (lst->userdata != NULL || lst->userdata2 != NULL) {
        msg.userdata = lst->userdata;
        msg.userdata2 = lst->userdata2;
        msg.data = NULL; // this is a signal that the listener should clean up
        lst->fn(tf, &msg); // return value is ignored here - use TF_STAY or TF_CLOSE
    }

    lst->fn = NULL; // Discard listener

    if (i == tf->count_id_lst - 1) {
        tf->count_id_lst--;
    }
}

/** Clean up Type listener */
static inline void _TF_FN cleanup_type_listener(TinyFrame *tf, TF_COUNT i, struct TF_TypeListener_ *lst)
{
    lst->fn = NULL; // Discard listener
    if (i == tf->count_type_lst - 1) {
        tf->count_type_lst--;
    }
}

/** Clean up Generic listener */
static inline void _TF_FN cleanup_generic_listener(TinyFrame *tf, TF_COUNT i, struct TF_GenericListener_ *lst)
{
    lst->fn = NULL; // Discard listener
    if (i == tf->count_generic_lst - 1) {
        tf->count_generic_lst--;
    }
}

/** Add a new ID listener. Returns 1 on success. */
bool _TF_FN TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_TICKS timeout)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < TF_MAX_ID_LST; i++) {
        lst = &tf->id_listeners[i];
        // test for empty slot
        if (lst->fn == NULL) {
            lst->fn = cb;
            lst->id = msg->frame_id;
            lst->userdata = msg->userdata;
            lst->userdata2 = msg->userdata2;
            lst->timeout_max = lst->timeout = timeout;
            if (i >= tf->count_id_lst) {
                tf->count_id_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add ID listener");
    return false;
}

/** Add a new Type listener. Returns 1 on success. */
bool _TF_FN TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_TypeListener_ *lst;
    for (i = 0; i < TF_MAX_TYPE_LST; i++) {
        lst = &tf->type_listeners[i];
        // test for empty slot
        if (lst->fn == NULL) {
            lst->fn = cb;
            lst->type = frame_type;
            if (i >= tf->count_type_lst) {
                tf->count_type_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add type listener");
    return false;
}

/** Add a new Generic listener. Returns 1 on success. */
bool _TF_FN TF_AddGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_GenericListener_ *lst;
    for (i = 0; i < TF_MAX_GEN_LST; i++) {
        lst = &tf->generic_listeners[i];
        // test for empty slot
        if (lst->fn == NULL) {
            lst->fn = cb;
            if (i >= tf->count_generic_lst) {
                tf->count_generic_lst = (TF_COUNT) (i + 1);
            }
            return true;
        }
    }

    TF_Error("Failed to add generic listener");
    return false;
}

/** Remove a ID listener by its frame ID. Returns 1 on success. */
bool _TF_FN TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < tf->count_id_lst; i++) {
        lst = &tf->id_listeners[i];
        // test if live & matching
        if (lst->fn != NULL && lst->id == frame_id) {
            cleanup_id_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("ID listener %d to remove not found", (int)frame_id);
    return false;
}

/** Remove a type listener by its type. Returns 1 on success. */
bool _TF_FN TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type)
{
    TF_COUNT i;
    struct TF_TypeListener_ *lst;
    for (i = 0; i < tf->count_type_lst; i++) {
        lst = &tf->type_listeners[i];
        // test if live & matching
        if (lst->fn != NULL    && lst->type == type) {
            cleanup_type_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Type listener %d to remove not found", (int)type);
    return false;
}

/** Remove a generic listener by its function pointer. Returns 1 on success. */
bool _TF_FN TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb)
{
    TF_COUNT i;
    struct TF_GenericListener_ *lst;
    for (i = 0; i < tf->count_generic_lst; i++) {
        lst = &tf->generic_listeners[i];
        // test if live & matching
        if (lst->fn == cb) {
            cleanup_generic_listener(tf, i, lst);
            return true;
        }
    }

    TF_Error("Generic listener to remove not found");
    return false;
}

/** Handle a message that was just collected & verified by the parser */
static void _TF_FN TF_HandleReceivedMessage(TinyFrame *tf)
{
    TF_COUNT i;
    struct TF_IdListener_ *ilst;
    struct TF_TypeListener_ *tlst;
    struct TF_GenericListener_ *glst;
    TF_Result res;

    // Prepare message object
    TF_Msg msg;
    TF_ClearMsg(&msg);
    msg.frame_id = tf->id;
    msg.is_response = false;
    msg.type = tf->type;
    msg.data = tf->data;

    // Trina solar message object
    msg.soi = tf->soi;
    msg.pversion = tf->pversion;
    msg.adr = tf->adr;
    msg.cid1 = tf->cid1;
    msg.cid2 = tf->cid2;
    msg.len = tf->len;

    msg.eoi = tf->eoi;

    // Any listener can consume the message, or let someone else handle it.

    // The loop upper bounds are the highest currently used slot index
    // (or close to it, depending on the order of listener removals).

    // ID listeners first
    for (i = 0; i < tf->count_id_lst; i++) {
        ilst = &tf->id_listeners[i];

        if (ilst->fn && ilst->id == msg.frame_id) {
            msg.userdata = ilst->userdata; // pass userdata pointer to the callback
            msg.userdata2 = ilst->userdata2;
            res = ilst->fn(tf, &msg);
            ilst->userdata = msg.userdata; // put it back (may have changed the pointer or set to NULL)
            ilst->userdata2 = msg.userdata2; // put it back (may have changed the pointer or set to NULL)

            if (res != TF_NEXT) {
                // if it's TF_CLOSE, we assume user already cleaned up userdata
                if (res == TF_RENEW) {
                    renew_id_listener(ilst);
                }
                else if (res == TF_CLOSE) {
                    // Set userdata to NULL to avoid calling user for cleanup
                    ilst->userdata = NULL;
                    ilst->userdata2 = NULL;
                    cleanup_id_listener(tf, i, ilst);
                }
                return;
            }
        }
    }
    // clean up for the following listeners that don't use userdata (this avoids data from
    // an ID listener that returned TF_NEXT from leaking into Type and Generic listeners)
    msg.userdata = NULL;
    msg.userdata2 = NULL;

    // Type listeners
    for (i = 0; i < tf->count_type_lst; i++) {
        tlst = &tf->type_listeners[i];

        if (tlst->fn && tlst->type == msg.type) {
            res = tlst->fn(tf, &msg);

            if (res != TF_NEXT) {
                // type listeners don't have userdata.
                // TF_RENEW doesn't make sense here because type listeners don't expire = same as TF_STAY

                if (res == TF_CLOSE) {
                    cleanup_type_listener(tf, i, tlst);
                }
                return;
            }
        }
    }

    // Generic listeners
    for (i = 0; i < tf->count_generic_lst; i++) {
        glst = &tf->generic_listeners[i];

        if (glst->fn) {
            res = glst->fn(tf, &msg);

            if (res != TF_NEXT) {
                // generic listeners don't have userdata.
                // TF_RENEW doesn't make sense here because generic listeners don't expire = same as TF_STAY

                // note: It's not expected that user will have multiple generic listeners, or
                // ever actually remove them. They're most useful as default callbacks if no other listener
                // handled the message.

                if (res == TF_CLOSE) {
                    cleanup_generic_listener(tf, i, glst);
                }
                return;
            }
        }
    }

    TF_Error("Unhandled message, type %d", (int)msg.type);
}

/** Externally renew an ID listener */
bool _TF_FN TF_RenewIdListener(TinyFrame *tf, TF_ID id)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;
    for (i = 0; i < tf->count_id_lst; i++) {
        lst = &tf->id_listeners[i];
        // test if live & matching
        if (lst->fn != NULL && lst->id == id) {
            renew_id_listener(lst);
            return true;
        }
    }

    TF_Error("Renew listener: not found (id %d)", (int)id);
    return false;
}

//endregion Listeners


//region Parser

/** Handle a received byte buffer */
void _TF_FN TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++) {
        TF_AcceptChar(tf, buffer[i]);
    }
}

/** Reset the parser's internal state. */
void _TF_FN TF_ResetParser(TinyFrame *tf)
{
    tf->state = TFState_SOF;
    // more init will be done by the parser when the first byte is received
}

/** SOF was received - prepare for the frame */
static void _TF_FN pars_begin_frame(TinyFrame *tf) {
    // Reset state vars
    CKSUM_RESET(tf->cksum);
#if TF_USE_SOF_BYTE
    CKSUM_ADD(tf->cksum, TF_SOF_BYTE);
#endif

    tf->discard_data = false;

    // Enter ID state
    tf->state = TFState_ID;
    tf->rxi = 0;
}

/** Handle a received char - here's the main state machine */
void _TF_FN TF_AcceptChar(TinyFrame *tf, unsigned char c)
{
    // Parser timeout - clear
    if (tf->parser_timeout_ticks >= TF_PARSER_TIMEOUT_TICKS) {
        if (tf->state != TFState_SOF) {
            TF_ResetParser(tf);
            TF_Error("Parser timeout");
        }
    }
    tf->parser_timeout_ticks = 0;

// DRY snippet - collect multi-byte number from the input stream, byte by byte
// This is a little dirty, but makes the code easier to read. It's used like e.g. if(),
// the body is run only after the entire number (of data type 'type') was received
// and stored to 'dest'
#define COLLECT_NUMBER(dest, type) dest = (type)(((dest) << 8) | c); \
                                   if (++tf->rxi == sizeof(type))

#if !TF_USE_SOF_BYTE
    if (tf->state == TFState_SOF) {
        pars_begin_frame(tf);
    }
#endif

    //@formatter:off
    switch (tf->state) {
        case TFState_SOF:
            if (c == TF_SOF_BYTE) {
                pars_begin_frame(tf);
            }
            break;

        case TFState_ID:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->id, TF_ID) {
                // Enter LEN state
                tf->state = TFState_LEN;
                tf->rxi = 0;
            }
            break;

        case TFState_LEN:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->len, TF_LEN) {
                // Enter TYPE state
                tf->state = TFState_TYPE;
                tf->rxi = 0;
            }
            break;

        case TFState_TYPE:
            CKSUM_ADD(tf->cksum, c);
            COLLECT_NUMBER(tf->type, TF_TYPE) {
                #if TF_CKSUM_TYPE == TF_CKSUM_NONE
                    tf->state = TFState_DATA;
                    tf->rxi = 0;
                #else
                    // enter HEAD_CKSUM state
                    tf->state = TFState_HEAD_CKSUM;
                    tf->rxi = 0;
                    tf->ref_cksum = 0;
                #endif
            }
            break;

        case TFState_HEAD_CKSUM:
            COLLECT_NUMBER(tf->ref_cksum, TF_CKSUM) {
                // Check the header checksum against the computed value
                CKSUM_FINALIZE(tf->cksum);

                if (tf->cksum != tf->ref_cksum) {
                    TF_Error("Rx head cksum mismatch");
                    TF_ResetParser(tf);
                    break;
                }

                if (tf->len == 0) {
                    // if the message has no body, we're done.
                    TF_HandleReceivedMessage(tf);
                    TF_ResetParser(tf);
                    break;
                }

                // Enter DATA state
                tf->state = TFState_DATA;
                tf->rxi = 0;

                CKSUM_RESET(tf->cksum); // Start collecting the payload

                if (tf->len > TF_MAX_PAYLOAD_RX) {
                    TF_Error("Rx payload too long: %d", (int)tf->len);
                    // ERROR - frame too long. Consume, but do not store.
                    tf->discard_data = true;
                }
            }
            break;

        case TFState_DATA:
            if (tf->discard_data) {
                tf->rxi++;
            } else {
                CKSUM_ADD(tf->cksum, c);
                tf->data[tf->rxi++] = c;
            }

            if (tf->rxi == tf->len) {
                #if TF_CKSUM_TYPE == TF_CKSUM_NONE
                    // All done
                    TF_HandleReceivedMessage(tf);
                    TF_ResetParser(tf);
                #else
                    // Enter DATA_CKSUM state
                    tf->state = TFState_DATA_CKSUM;
                    tf->rxi = 0;
                    tf->ref_cksum = 0;
                #endif
            }
            break;

        case TFState_DATA_CKSUM:
            COLLECT_NUMBER(tf->ref_cksum, TF_CKSUM) {
                // Check the header checksum against the computed value
                CKSUM_FINALIZE(tf->cksum);
                if (!tf->discard_data) {
                    if (tf->cksum == tf->ref_cksum) {
                        TF_HandleReceivedMessage(tf);
                    } else {
                        TF_Error("Body cksum mismatch");
                    }
                }

                TF_ResetParser(tf);
            }
            break;
    }
    //@formatter:on
}

//endregion Parser

/** Write a number to the output buffer. */
#define WRITENUM_BASE(type, num, xtra) \
    for (si = sizeof(type)-1; si>=0; si--) { \
        b = (uint8_t)((num) >> (si*8) & 0xFF); \
        outbuff[pos++] = b; \
        xtra; \
    }

/** Do nothing */
#define _NOOP()

/** Write a number without adding its bytes to the checksum */
#define WRITENUM(type, num)       WRITENUM_BASE(type, num, _NOOP())

/** Write a number AND add its bytes to the checksum */
#define WRITENUM_CKSUM(type, num) WRITENUM_BASE(type, num, CKSUM_ADD(cksum, b))

/** Compose a frame header (used internally by TF_Send and TF_Respond). */
static inline uint64_t _TF_FN TF_ComposeHead(TinyFrame *tf, uint16_t *outbuff, TF_Msg *msg)
{
    int8_t si = 0; // signed small int
    uint8_t b = 0;
    //TF_ID id = 0;
    TF_CKSUM cksum = 0;
    uint64_t pos = 0;
    TF_ADR adr = 0;  // Trina solar device adr
    
    (void)cksum; // suppress "unused" warning if checksums are disabled

    CKSUM_RESET(cksum);

    // ADR
    if (msg->is_response) {
        adr = msg->adr;  
        // not sure what's the response format name now, should be the same as command adr
    }

    if (msg->is_command){
        adr = msg->adr;  // send command device adr
    }

    msg->adr = adr; // put the resolved ID into the message object for later use

    // --- Start ---
    CKSUM_RESET(cksum);

    // Trina solar SOI byte
    outbuff[pos++] = TF_SOI_BYTE;
    CKSUM_ADD(cksum, TF_SOI_BYTE);

    // Trina solar protocol 1 version
    outbuff[pos++] = PVERSION_1_BYTE;
    CKSUM_ADD(cksum, PVERSION_1_BYTE);

    WRITENUM_CKSUM(TF_ADR, id);
    WRITENUM_CKSUM(TF_CID1, msg->cid1);
    WRITENUM_CKSUM(TF_CID2, msg->cid2);

    CKSUM_FINALIZE(cksum);
    WRITENUM(TF_CKSUM, cksum);

    return pos;
}

/** Compose a frame body (used internally by TF_Send and TF_Respond). */
static inline uint32_t _TF_FN TF_ComposeBody(uint8_t *outbuff,
                                    const uint8_t *data, TF_LEN data_len,
                                    TF_CKSUM *cksum)
{
    TF_LEN i = 0;
    uint8_t b = 0;
    uint32_t pos = 0;

    for (i = 0; i < data_len; i++) {
        b = data[i];
        outbuff[pos++] = b;
        CKSUM_ADD(*cksum, b);
    }

    return pos;
}

/** Finalize a frame */
static inline uint32_t _TF_FN TF_ComposeTail(uint8_t *outbuff, TF_CKSUM *cksum)
{
    int8_t si = 0; // signed small int
    uint8_t b = 0;
    uint32_t pos = 0;

    // Trina solar EOI byte
    outbuff[pos++] = TF_EOI_BYTE;
    CKSUM_ADD(*cksum, TF_EOI_BYTE);

    CKSUM_FINALIZE(*cksum);
    WRITENUM(TF_CKSUM, *cksum);

    return pos;
}

/** Begin building and sending a frame */
static bool _TF_FN TF_SendFrame_Begin(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_TICKS timeout)
{
    TF_TRY(TF_ClaimTx(tf));

    tf->tx_pos = (uint64_t) TF_ComposeHead(tf, tf->sendbuf, msg);
    tf->tx_len = msg->len;

    if (listener) {
        TF_TRY(TF_AddIdListener(tf, msg, listener, timeout));
    }

    CKSUM_RESET(tf->tx_cksum);
    return true;
}

/** Build and send a part (or all) of a frame body. */
static void _TF_FN TF_SendFrame_Chunk(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    uint32_t remain;
    uint32_t chunk;
    uint32_t sent = 0;

    remain = length;
    while (remain > 0) {
        // Write what can fit in the tx buffer
        chunk = TF_MIN(TF_SENDBUF_LEN - tf->tx_pos, remain);
        tf->tx_pos += TF_ComposeBody(tf->sendbuf+tf->tx_pos, buff+sent, (TF_LEN) chunk, &tf->tx_cksum);
        remain -= chunk;
        sent += chunk;

    }
}

/** End a multi-part frame. This sends the checksum and releases mutex. */
static void _TF_FN TF_SendFrame_End(TinyFrame *tf)
{
    // Checksum only if message had a body
    if (tf->tx_len > 0) {
        // Add checksum, flush what remains to be sent
        tf->tx_pos += TF_ComposeTail(tf->sendbuf + tf->tx_pos, &tf->tx_cksum);
    }
    TF_WriteImpl(tf, (const uint8_t *) tf->sendbuf, tf->tx_pos);
    TF_ReleaseTx(tf);
}

/** Send a message */
static bool _TF_FN TF_SendFrame(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_TICKS timeout)
{
    TF_TRY(TF_SendFrame_Begin(tf, msg, listener, timeout));
    if (msg->len == 0 || msg->data != NULL) {
        TF_SendFrame_Chunk(tf, msg->data, msg->len);
        TF_SendFrame_End(tf);
    }
    return true;
}
//endregion Compose and send


//region Sending API funcs

/** send without listener, NULL listener */
bool _TF_FN TF_Send(TinyFrame *tf, TF_Msg *msg)
{
    return TF_SendFrame(tf, msg, NULL, 0);
}

/** send with a listener waiting for a reply */
bool _TF_FN TF_Query(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_TICKS timeout)
{
    return TF_SendFrame(tf, msg, listener, timeout);
}

/** Like TF_Send, but with explicit frame ID (set inside the msg object), use for responses */
bool _TF_FN TF_Respond(TinyFrame *tf, TF_Msg *msg)
{
    msg->is_response = true;
    return TF_Send(tf, msg);
}

//endregion Sending API funcs


//region Sending API funcs - multipart

bool _TF_FN TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = NULL;
    return TF_Send(tf, msg);
}

bool _TF_FN TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_TICKS timeout)
{
    msg->data = NULL;
    return TF_Query(tf, msg, listener, timeout);
}

void _TF_FN TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg)
{
    msg->data = NULL;
    TF_Respond(tf, msg);
}

void _TF_FN TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length)
{
    TF_SendFrame_Chunk(tf, buff, length);
}

void _TF_FN TF_Multipart_Close(TinyFrame *tf)
{
    TF_SendFrame_End(tf);
}

//endregion Sending API funcs - multipart


/** Timebase hook - for timeouts */
void _TF_FN TF_Tick(TinyFrame *tf)
{
    TF_COUNT i;
    struct TF_IdListener_ *lst;

    // increment parser timeout (timeout is handled when receiving next byte)
    if (tf->parser_timeout_ticks < TF_PARSER_TIMEOUT_TICKS) {
        tf->parser_timeout_ticks++;
    }

    // decrement and expire ID listeners
    for (i = 0; i < tf->count_id_lst; i++) {
        lst = &tf->id_listeners[i];
        if (!lst->fn || lst->timeout == 0) continue;
        // count down...
        if (--lst->timeout == 0) {
            TF_Error("ID listener %d has expired", (int)lst->id);
            // Listener has expired
            cleanup_id_listener(tf, i, lst);
        }
    }
}
