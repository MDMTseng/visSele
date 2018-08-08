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
        error_data.type = DatCH_DataType_error;
        error_data.data.error.code = code;
        return error_data;
    }
    void SetEventCallBack(DatCH_Event_callback callback, void* callback_param)
    {
        this->callback = callback;
        this->callback_param = callback_param;
    }

    virtual void* GetData()
    {
        return NULL;
    }
};




#endif
