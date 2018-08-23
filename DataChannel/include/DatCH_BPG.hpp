#ifndef DATCH_BPG_HPP
#define DATCH_BPG_HPP

#include <DatCH.hpp>
#include <string>

typedef struct BPG_data
{
    char type[2];//Two letter
    uint8_t prop;
    uint32_t size;//32bit
    uint8_t*     raw;
};




class DatCH_BPG: public DatCH_Interface
{
protected:
    std::string version;
    BPG_data data;
public:
    DatCH_BPG(){
      version="NULL";
      //data.type = DatCH_Data::DataType_BP;
    }
    ~DatCH_BPG()
    {

    }
    //DatCH_Data SendData(DatCH_Data) override;
    DatCH_Data SendHR()
    {
        /*data.type = DatCH_Data::DataType_BPG;
        data.data.p_BPG_data =
        if (callback != NULL)
        {
            callback(this, data, callback_param);
        }
        return data;*/
    }
};


class DatCH_BPG1_0: public DatCH_BPG
{
protected:
public:
    DatCH_BPG1_0();
    ~DatCH_BPG1_0();
    //DatCH_Data SendData(DatCH_Data) override;
    DatCH_Data SendHR();
};


#endif
