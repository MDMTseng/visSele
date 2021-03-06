#ifndef DATCH_IMAGE_HPP
#define DATCH_IMAGE_HPP


#include <string>
#include <DatCH.hpp>

class DatCH_acvImageInterface: public DatCH_Interface
{
protected:
    acvImage *buffer;
public:
    DatCH_acvImageInterface(acvImage *buffer): DatCH_Interface()
    {
        this->buffer = buffer;
    }

    virtual acvImage* GetAcvImage()
    {
        return buffer;
    }
};

class DatCH_BMP: public DatCH_acvImageInterface
{
    std::string fileName;
public:
    DatCH_BMP(acvImage *buffer): DatCH_acvImageInterface(buffer)
    {
        fileName = "";
    }

    void SetFileName(std::string fileName)
    {
        this->fileName = fileName;

        if (cb_obj != NULL)
        {
            GetAcvImage();
        }
    }

    DatCH_Data GetData()
    {

        DatCH_Data img_data;
        img_data.type = DatCH_Data::DataType_BMP_Read;
        img_data.data.BMP_Read.img = buffer;
        return img_data;
    }

    acvImage* GetAcvImage()
    {
        if (buffer == NULL && cb_obj)
        {
            cb_obj->callback(this, GenErrorMsg(DatCH_Data_error::error_enum::NO_ENOUGH_BUFFER), callback_param);
            return NULL;
        }

        if (fileName.empty() && cb_obj)
        {
            cb_obj->callback(this, GenErrorMsg(DatCH_Data_error::error_enum::UNKNOWN_FILE_NAME), callback_param);
            return NULL;
        }
        int ret = acvLoadBitmapFile(buffer, fileName.c_str());
        if (ret < 0 && cb_obj)
        {
            cb_obj->callback(this, GenErrorMsg(DatCH_Data_error::error_enum::FILE_READ_FAILED), callback_param);
            return NULL;
        }

        if (cb_obj)
        {
            cb_obj->callback(this, GetData(), callback_param);
        }
        return buffer;
    }
};

#endif
