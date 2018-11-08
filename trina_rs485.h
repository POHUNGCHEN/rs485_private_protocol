typedef struct TR_Msg_{
    char _cl_port;
    // **** Trina Solor protocol data start
    int _cl_soi_byte;
    int _cl_pversion_byte;
    int _cl_adr_byte;
    int _cl_cid1_byte;
    int _cl_cid2_byte;
    int _cl_len1_byte;
    int _cl_len2_byte;

    // **** Trina Solor protocol DATA INFO
    int _cl_DataInfo1_byte;
    int _cl_DataInfo2_byte;
    int _cl_DataInfo3_byte;
    int _cl_DataInfo4_byte;

    int _cl_ckcsum1_byte;
    int _cl_ckcsum2_byte;
    int _cl_eoi_byte;
    // **** Trina Solor protocol data end
}TR_Msg;

int mx_rs485_new(char * _cl_port);

int mx_rs485_connect(TR_Msg *tr);

int mx_rs485_reconnect(TR_Msg *tr);

int mx_rs485_close();

