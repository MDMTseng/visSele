#ifndef DATCH_HPP
#define DATCH_HPP

#include <DatCH_structs.hpp>

class DatCH_Interface
{
protected:
    DatCH_Event_callback callback;
    void* callback_param;
public:
    DatCH_Interface()
    {
        callback = NULL;
        callback_param = NULL;
    }


    DatCH_Data GenErrorMsg(DatCH_Data_error::error_enum code)
    {
        DatCH_Data error_data;
        error_data.type = DatCH_Data::DataType_error;
        error_data.data.error.code = code;
        return error_data;
    }


    DatCH_Data GenMsgType(DatCH_Data::TYPE type)
    {
        DatCH_Data data;
        data.type = type;
        data.data.raw = NULL;
        return data;
    }

    void SetEventCallBack(DatCH_Event_callback callback, void* callback_param)
    {
        this->callback = callback;
        this->callback_param = callback_param;
    }

    virtual DatCH_Data GetData()
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }
    
    virtual DatCH_Data SendData(void* data, size_t dataL)
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }

    virtual DatCH_Data SendData(DatCH_Data)
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }
};




#endif
