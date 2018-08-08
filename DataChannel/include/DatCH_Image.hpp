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
    string fileName;
public:
    DatCH_BMP(acvImage *buffer): DatCH_acvImageInterface(buffer)
    {
        fileName = "";
    }

    void SetFileName(string fileName)
    {
        this->fileName = fileName;

        if (callback != NULL)
        {
            GetAcvImage();
        }
    }

    void* GetData()
    {
        return GetAcvImage();
    }

    acvImage* GetAcvImage()
    {
        if (buffer == NULL)
        {
            callback(this, GenErrorMsg(DatCH_Data_error::error_enum::NO_ENOUGH_BUFFER), callback_param);
            return NULL;
        }

        if (fileName.empty() )
        {
            callback(this, GenErrorMsg(DatCH_Data_error::error_enum::UNKNOWN_FILE_NAME), callback_param);
            return NULL;
        }
        int ret = acvLoadBitmapFile(buffer, fileName.c_str());
        if (ret < 0)
        {
            callback(this, GenErrorMsg(DatCH_Data_error::error_enum::FILE_READ_FAILED), callback_param);
            return NULL;
        }

        if (callback != NULL)
        {
            DatCH_Data img_data;
            img_data.type = DatCH_DataType_BMP_Read;
            img_data.data.BMP_Read.img = buffer;
            callback(this, img_data, callback_param);
        }
        return buffer;
    }
};

#endif
