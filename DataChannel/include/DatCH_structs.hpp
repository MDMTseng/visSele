#ifndef DATCH_STRUCTS_HPP
#define DATCH_STRUCTS_HPP

#include <acvImage_BasicTool.hpp>
class DatCH_Interface;

typedef struct DatCH_Data_error
{

    enum error_enum
    {
        NO_ENOUGH_BUFFER,
        FILE_READ_FAILED,
        UNKNOWN_FILE_NAME,
        NOT_SUPPORTED,
        SEND_ERROR,
        NO_CALLBACK_ERROR,
    } code;

};

typedef struct DatCH_Data_BMP_Read
{
    acvImage* img;
};
typedef struct websock_data;
typedef struct BPG_data;

typedef struct DatCH_Data_Fragment
{
    uint32_t maxFrameL;
    uint32_t offset;
    bool finish;
    uint32_t dataL;
    uint8_t *data;
};




typedef struct DatCH_Data
{
    enum TYPE{
      DataType_NULL,
      DataType_ACK,
      DataType_NAK,
      DataType_error,
      DataType_raw,
      DataType_BMP_Read,
      DataType_websock_data,
      DataType_BPG,
      DataType_Fragment,
      DataType_END,
    } type;
    union data
    {
        void*     raw;
        DatCH_Data_error error;
        DatCH_Data_BMP_Read BMP_Read;
        websock_data* p_websocket;
        BPG_data* p_BPG_data;
    } data;
} DatCH_Data;

typedef int (*DatCH_Event_callback)(DatCH_Interface *interface, DatCH_Data data, void* callback_param);


#endif
