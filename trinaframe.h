#ifndef TinyFrameH
#define TinyFrameH

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>  

// Checksum type (0 = none, 8 = ~XOR, 16 = CRC16 0x8005, 32 = CRC32)
#define TF_CKSUM_NONE  0  // no checksums
#define TF_CKSUM_XOR   8  // inverted xor of all payload bytes
#define TF_CKSUM_CRC8  9  // Dallas/Maxim CRC8 (1-wire)
#define TF_CKSUM_CRC16 16 // CRC16 with the polynomial 0x8005 (x^16 + x^15 + x^2 + 1)
#define TF_CKSUM_CRC32 32 // CRC32 with the polynomial 0xedb88320

#define TF_CKSUM_TYPE TF_CKSUM_CUSTOM16
#define TF_CKSUM_CUSTOM16 2  // 16-bit checksum

#include "TF_Config.h"

//Trina command data length 4 bytes
typedef uint32_t TF_LEN;

#if TF_TYPE_BYTES == 1
    typedef uint8_t TF_TYPE;
#elif TF_TYPE_BYTES == 2
    typedef uint16_t TF_TYPE;
#elif TF_TYPE_BYTES == 4
    typedef uint32_t TF_TYPE;
#else
    #error Bad value of TF_TYPE_BYTES, must be 1, 2 or 4
#endif


#if TF_ID_BYTES == 1
    typedef uint8_t TF_ID;
#elif TF_ID_BYTES == 2
    typedef uint16_t TF_ID;
#elif TF_ID_BYTES == 4
    typedef uint32_t TF_ID;
#else
    #error Bad value of TF_ID_BYTES, must be 1, 2 or 4
#endif

// Trina solar ADR, CID1/2 byte define
typedef uint8_t TF_ADR;
typedef uint8_t TF_CID1;
typedef uint8_t TF_CID2;


#if (TF_CKSUM_TYPE == TF_CKSUM_XOR) || (TF_CKSUM_TYPE == TF_CKSUM_NONE) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM8) || (TF_CKSUM_TYPE == TF_CKSUM_CRC8)
    // ~XOR (if 0, still use 1 byte - it won't be used)
    typedef uint8_t TF_CKSUM;
#elif (TF_CKSUM_TYPE == TF_CKSUM_CRC16) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM16)
    // CRC16
    typedef uint16_t TF_CKSUM;
#elif (TF_CKSUM_TYPE == TF_CKSUM_CRC32) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM32)
    // CRC32
    typedef uint32_t TF_CKSUM;
#else
    #error Bad value for TF_CKSUM_TYPE
#endif

//endregion

//---------------------------------------------------------------------------

/** Peer bit enum (used for init) */
typedef enum {
    TF_SLAVE = 0,
    TF_MASTER = 1,
} TF_Peer;


/** Response from listeners */
typedef enum {
    TF_NEXT = 0,   //!< Not handled, let other listeners handle it
    TF_STAY = 1,   //!< Handled, stay
    TF_RENEW = 2,  //!< Handled, stay, renew - useful only with listener timeout
    TF_CLOSE = 3,  //!< Handled, remove self
} TF_Result;


/** Data structure for sending / receiving messages */
typedef struct TF_Msg_ {
    //Trina solar fixed part
    TF_SOI soi;           //Trina solar SOI
    TF_EOI eoi;           //Trina solar EOI
    TF_PVERSION pversion; //Trina solar protocol version

    // Trina solar header contains ADR, CID1, CID2
    TF_ADR adr;           //Trina solar ADR
    TF_CID1 cid1;         //Trina solar CID1
    TF_CID2 cid2;         //Trina solar CID2
    bool is_command;      //Trina solar check if this is command mode

    TF_ID frame_id;       //!< message ID
    bool is_response;     //!< internal flag, set when using the Respond function. frame_id is then kept unchanged.
    TF_TYPE type;         //!< received or sent message type

    const uint8_t *data;
    TF_LEN len; //!< length of the payload

    /** Custom user data for the ID listener. */
    void *userdata;
    void *userdata2;
} TF_Msg;

/** Clear message struct */
static inline void TF_ClearMsg(TF_Msg *msg)
{
    memset(msg, 0, sizeof(TF_Msg));
}

/** TinyFrame struct typedef */
typedef struct TinyFrame_ TinyFrame;

/** TinyFrame Type Listener callback */
typedef TF_Result (*TF_Listener)(TinyFrame *tf, TF_Msg *msg);

// ---------------------------------- INIT ------------------------------

/** Initialize the TinyFrame engine. */
TinyFrame *TF_Init(TF_Peer peer_bit);

/** Initialize the TinyFrame engine using a statically allocated instance struct.*/
bool TF_InitStatic(TinyFrame *tf, TF_Peer peer_bit);

/** De-init the dynamically allocated TF instance */
void TF_DeInit(TinyFrame *tf);

// ---------------------------------- API CALLS --------------------------------------

/** Accept incoming bytes & parse frames */
void TF_Accept(TinyFrame *tf, const uint8_t *buffer, uint32_t count);

/** Accept a single incoming byte */
void TF_AcceptChar(TinyFrame *tf, uint8_t c);

/** This function should be called periodically. */
void TF_Tick(TinyFrame *tf);

/** Reset the frame parser state machine. */
void TF_ResetParser(TinyFrame *tf);

// ---------------------------- MESSAGE LISTENERS -------------------------------

/** Register a frame type listener. */
bool TF_AddIdListener(TinyFrame *tf, TF_Msg *msg, TF_Listener cb, TF_TICKS timeout);

/** Remove a listener by the message ID it's registered for */
bool TF_RemoveIdListener(TinyFrame *tf, TF_ID frame_id);

/** Register a frame type listener. */
bool TF_AddTypeListener(TinyFrame *tf, TF_TYPE frame_type, TF_Listener cb);

/** Remove a listener by type. */
bool TF_RemoveTypeListener(TinyFrame *tf, TF_TYPE type);

/** Register a generic listener. */
bool TF_AddGenericListener(TinyFrame *tf, TF_Listener cb);

/** Remove a generic listener by function pointer */
bool TF_RemoveGenericListener(TinyFrame *tf, TF_Listener cb);

/** Renew an ID listener timeout externally */
bool TF_RenewIdListener(TinyFrame *tf, TF_ID id);

// ---------------------------- FRAME TX FUNCTIONS ------------------------------

/** Send a frame, no listener */
bool TF_Send(TinyFrame *tf, TF_Msg *msg);

/** Like TF_Send, but without the struct */
bool TF_SendSimple(TinyFrame *tf, TF_TYPE type, const uint8_t *data, TF_LEN len);

/** Send a frame, and optionally attach an ID listener. */
bool TF_Query(TinyFrame *tf, TF_Msg *msg,
              TF_Listener listener, TF_TICKS timeout);

/** Like TF_Query(), but without the struct */
bool TF_QuerySimple(TinyFrame *tf, TF_TYPE type,
                    const uint8_t *data, TF_LEN len,
                    TF_Listener listener, TF_TICKS timeout);

/** Send a response to a received message. */
bool TF_Respond(TinyFrame *tf, TF_Msg *msg);

// ------------------------ MULTIPART FRAME TX FUNCTIONS -----------------------------
// Those routines are used to send long frames without having all the data available
// at once (e.g. capturing it from a peripheral or reading from a large memory buffer)

/**
 * TF_Send() with multipart payload.
 * msg.data is ignored and set to NULL
 */
bool TF_Send_Multipart(TinyFrame *tf, TF_Msg *msg);

/**
 * TF_SendSimple() with multipart payload.
 */
bool TF_SendSimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len);

/**
 * TF_QuerySimple() with multipart payload.
 */
bool TF_QuerySimple_Multipart(TinyFrame *tf, TF_TYPE type, TF_LEN len, TF_Listener listener, TF_TICKS timeout);

/**
 * TF_Query() with multipart payload.
 * msg.data is ignored and set to NULL
 */
bool TF_Query_Multipart(TinyFrame *tf, TF_Msg *msg, TF_Listener listener, TF_TICKS timeout);

/**
 * TF_Respond() with multipart payload.
 * msg.data is ignored and set to NULL
 */
void TF_Respond_Multipart(TinyFrame *tf, TF_Msg *msg);

/** Send the payload for a started multipart frame. This can be called multiple times */
void TF_Multipart_Payload(TinyFrame *tf, const uint8_t *buff, uint32_t length);

/** Close the multipart message, generating chekcsum and releasing the Tx lock. */
void TF_Multipart_Close(TinyFrame *tf);

// ---------------------------------- INTERNAL ----------------------------------
// This is publicly visible only to allow static init.

enum TF_State_ {
    TFState_SOF = 0,      //!< Wait for SOF
    TFState_LEN,          //!< Wait for Number Of Bytes
    TFState_HEAD_CKSUM,   //!< Wait for header Checksum
    TFState_ID,           //!< Wait for ID
    TFState_TYPE,         //!< Wait for message type
    TFState_DATA,         //!< Receive payload
    TFState_DATA_CKSUM    //!< Wait for Checksum
};

// Trina ADR information listener
struct TF_IdListener_ {
    TF_ID id;
    TF_ADR adr;             // trina solar device ADR
    TF_Listener fn;
    TF_TICKS timeout;     // nr of ticks remaining to disable this listener
    TF_TICKS timeout_max; // the original timeout is stored here (0 = no timeout)
    void *userdata;
    void *userdata2;
};

// Trina CID1 / CID2 information listener
struct TF_TypeListener_ {
    TF_TYPE type;
    TF_CID1 cid1;         //Trina solar CID2
    TF_CID2 cid2;         //Trina solar CID2
    TF_Listener fn;
};

struct TF_GenericListener_ {
    TF_Listener fn;
};

/** Frame parser internal state. */
struct TinyFrame_ {
    void *userdata;
    uint32_t usertag;

    /* Own state */
    TF_Peer peer_bit;       //!< Own peer bit (unqiue to avoid msg ID clash)
    TF_ID next_id;          //!< Next frame / frame chain ID

    /* Parser state */
    enum TF_State_ state;
    TF_TICKS parser_timeout_ticks;
    TF_ID id;               //!< Incoming packet ID

    TF_SOI soi;             // trina solar protocol soi
    TF_PVERSION pversion;   // trina solar protocol version
    TF_ADR adr;             // trina solar device ADR
    TF_CID1 cid1;         //Trina solar CID2
    TF_CID2 cid2;         //Trina solar CID2
    TF_EOI eoi;             // trina solar protocol eoi

    TF_LEN len;             //!< Payload length
    uint8_t data[TF_MAX_PAYLOAD_RX]; //!< Data byte buffer
    TF_LEN rxi;             //!< Field size byte counter
    TF_CKSUM cksum;         //!< Checksum calculated of the data stream
    TF_CKSUM ref_cksum;     //!< Reference checksum read from the message
    TF_TYPE type;           //!< Collected message type number

    bool discard_data;      //!< Set if (len > TF_MAX_PAYLOAD) to read the frame, but ignore the data.

    /* Tx state */
    // Buffer for building frames
    uint8_t sendbuf[TF_SENDBUF_LEN]; //!< Transmit temporary buffer

    uint32_t tx_pos;        //!< Next write position in the Tx buffer (used for multipart)
    uint64_t tx_len;        //!< Total expected Tx length
    TF_CKSUM tx_cksum;      //!< Transmit checksum accumulator

    bool soft_lock;         //!< Tx lock flag used if the mutex feature is not enabled.

    /* --- Callbacks --- */
    /* Transaction callbacks */
    struct TF_IdListener_ id_listeners[TF_MAX_ID_LST];
    struct TF_TypeListener_ type_listeners[TF_MAX_TYPE_LST];
    struct TF_GenericListener_ generic_listeners[TF_MAX_GEN_LST];

    TF_COUNT count_id_lst;
    TF_COUNT count_type_lst;
    TF_COUNT count_generic_lst;
};

//******************************************************************************
// ------------------------ TO BE IMPLEMENTED BY USER ------------------------
//******************************************************************************
/**
 * 'Write bytes' function that sends data to UART
 *
 * ! Implement this in your application code !
 */
extern void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len);

// Mutex functions
#if TF_USE_MUTEX

    /** Claim the TX interface before composing and sending a frame */
    extern bool TF_ClaimTx(TinyFrame *tf);

    /** Free the TX interface after composing and sending a frame */
    extern void TF_ReleaseTx(TinyFrame *tf);

#endif

// Custom checksum functions
#if (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM8) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM16) || (TF_CKSUM_TYPE == TF_CKSUM_CUSTOM32)

    /**
     * Initialize a checksum
     *
     * @return initial checksum value
     */
    extern TF_CKSUM TF_CksumStart(void);

    /**
     * Update a checksum with a byte
     *
     * @param cksum - previous checksum value
     * @param byte - byte to add
     * @return updated checksum value
     */
    extern TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte);

    /**
     * Finalize the checksum calculation
     *
     * @param cksum - previous checksum value
     * @return final checksum value
     */
    extern TF_CKSUM TF_CksumEnd(TF_CKSUM cksum);

#endif

#endif
