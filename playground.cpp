#include <playground.h>

void calcCameraCalibration()
{
    
    acvImage calibImage;
    acvImage labelImg;
    acvImage tmp;
    acvLoadBitmapFile(&calibImage,"data/calibration_Img/circleS.bmp");

    labelImg.ReSize(&calibImage);
    tmp.ReSize(&calibImage);
    
    acvCloneImage(&calibImage,&labelImg, -1);
    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvThreshold(&labelImg, 70);


    //Draw a labeling black cage for labling algo, which is needed for acvComponentLabeling
    acvDrawBlock(&labelImg, 1, 1, labelImg.GetWidth() - 2, labelImg.GetHeight() - 2);


    //The labeling starts from (1 1) => (W-2,H-2), ie. it will not touch the outmost pixel to simplify the boundary condition
    //You need to draw a black/white cage to work(not crash).
    //The advantage of black cage is you can know which area touches the boundary then we can exclude it
    
    vector<acv_LabeledData> ldData;
    acvComponentLabeling(&labelImg);
    acvLabeledRegionInfo(&labelImg, &ldData);

    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        LOGV(">>>%f, %f",ldat.Center.X,ldat.Center.Y);
    }



    acvSaveBitmapFile("data/labelImg.bmp",&labelImg);

    
}
