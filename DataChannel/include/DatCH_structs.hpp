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
    } code;

};

enum DatCH_DataType
{
    DatCH_DataType_error,
    DatCH_DataType_raw,
    DatCH_DataType_BMP_Read,
    DatCH_DataType_websock_data,
    DatCH_DataType_END,
};

typedef struct DatCH_Data_BMP_Read
{
    acvImage* img;
};
typedef struct websock_data;
typedef struct DatCH_Data
{
    enum DatCH_DataType type;
    union data
    {
        uint8_t*     raw;
        DatCH_Data_error error;
        DatCH_Data_BMP_Read BMP_Read;
        websock_data* p_websocket;
    } data;
} DatCH_Data;

typedef int (*DatCH_Event_callback)(DatCH_Interface *interface, DatCH_Data data, void* callback_param);


#endif
