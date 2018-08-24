#ifndef DATCH_HPP
#define DATCH_HPP

#include <DatCH_structs.hpp>

class DatCH_CallBack
{
public:
  virtual int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
  {
      return 0;
  }
};

class DatCH_Interface: public DatCH_CallBack
{
protected:
    DatCH_CallBack* cb_obj;
    void* callback_param;
public:
    DatCH_Interface()
    {
        cb_obj = NULL;
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

    void SetEventCallBack(DatCH_CallBack *callback_obj, void* callback_param)
    {
        this->cb_obj = callback_obj;
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
