#include "FeatureManager_sig360_circle_line.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <algorithm>
#include <limits>
#include <MatchingCore.h>
#include <stdio.h>
#include <acvImage_SpDomainTool.hpp>


static int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist);

int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,acv_XY *ret_point_opt,float *ret_edge_response);
int EdgePointOpt2(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int range,float thres,acv_XY *ret_point_opt,float *ret_edge_response);
/*
  FeatureManager_sig360_circle_line Section
*/
FeatureManager_sig360_circle_line::FeatureManager_sig360_circle_line(const char *json_str): FeatureManager_binary_processing(json_str)
{

  //LOGI(">>>>%s>>>>",json_str);
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
}

double* json_get_num(cJSON *root,char* path, char* dbg_str)
{
  double *pnum;
  if((pnum=(double *)JFetch(root,path,cJSON_Number)) == NULL )
  {
    if(dbg_str)
    {
      LOGE("%s: Cannot get Number In path: %s",dbg_str,path);
    }
    else
    {
      LOGE("Cannot get Number In path: %s",path);
    }
    return NULL;
  }
  return pnum;
}

double* json_get_num(cJSON *root,char* path)
{
  return json_get_num(root,path,NULL);
}

#define JSON_GET_NUM(JROOT,PATH) json_get_num(JROOT,PATH,(char*)__func__)


char* json_find_str(cJSON *root, const char* key)
{
  char *str;
  if(!(getDataFromJsonObj(root,key,(void**)&str)&cJSON_String))
  {
    return NULL;
  }
  return str;
}

char* json_find_name(cJSON *root)
{
  return json_find_str(root,"name");
}


int FeatureManager_sig360_circle_line::parse_arcData(cJSON * circle_obj)
{
  featureDef_circle cir;

  {
    char *tmpstr;
    cir.name[0] = '\0';
    if(tmpstr = json_find_name(circle_obj))
    {
      strcpy(cir.name,tmpstr);
    }
  }

  double *pnum;

  
  cir.id=(int)*JFetEx_NUMBER(circle_obj,"id");
  LOGV("feature is an arc:%s %d",cir.name, cir.id);



  cir.initMatchingMargin=*JFetEx_NUMBER(circle_obj,"margin");



  acv_XY pt1,pt2,pt3;
  
  pt1.X=*JFetEx_NUMBER(circle_obj,"pt1.x");
  pt1.Y=*JFetEx_NUMBER(circle_obj,"pt1.y");
  
  pt2.X=*JFetEx_NUMBER(circle_obj,"pt2.x");
  pt2.Y=*JFetEx_NUMBER(circle_obj,"pt2.y");
  
  pt3.X=*JFetEx_NUMBER(circle_obj,"pt3.x");
  pt3.Y=*JFetEx_NUMBER(circle_obj,"pt3.y");


  double direction = *JFetEx_NUMBER(circle_obj,"direction");
  
  acv_XY circumcenter = acvCircumcenter(pt1,pt2,pt3);
  cir.circleTar.circumcenter=circumcenter;
  cir.circleTar.radius=hypot(circumcenter.X-pt2.X,circumcenter.Y-pt2.Y);


  {
    pt1.X -= circumcenter.X;
    pt1.Y -= circumcenter.Y;
    pt2.X -= circumcenter.X;
    pt2.Y -= circumcenter.Y;
    pt3.X -= circumcenter.X;
    pt3.Y -= circumcenter.Y;
    float angle1 = atan2(pt1.Y,pt1.X);
    float angle2 = atan2(pt2.Y,pt2.X);
    float angle3 = atan2(pt3.Y,pt3.X);

    float angle21 = angle2-angle1;
    float angle31 = angle3-angle1;
    if(angle21<0)angle21+=2*M_PI;
    if(angle31<0)angle31+=2*M_PI;

    if(angle31>angle21)
    {
      cir.sAngle = angle1;
      cir.eAngle = angle3;
    }
    else
    {
      cir.sAngle = angle3;
      cir.eAngle = angle1;
    }
    cir.outter_inner=direction;
  }


  LOGV("x:%f y:%f r:%f margin:%f",
  cir.circleTar.circumcenter.X,
  cir.circleTar.circumcenter.Y,
  cir.circleTar.radius,
  cir.initMatchingMargin);
  if(cir.name[0]=='\0')
  {
    sprintf(cir.name,"@CIRCLE_%d",featureCircleList.size());
  }
  featureCircleList.push_back(cir);
  return 0;
}

/*
float FeatureManager_sig360_circle_line::find_search_key_points_longest_distance(vector<featureDef_line::searchKeyPoint> &skpsList)
{
  float maxDist=0;
  for(int i=0;i<skpsList.size()-1;i++)
  {
    for(int j=i+1;j<skpsList.size();j++)
    {
      float dist=acvDistance(skpsList[i].searchStart,skpsList[j].searchStart);
      if(maxDist<dist)
        maxDist=dist;
    }
  }
  return maxDist;
}*/

int FeatureManager_sig360_circle_line::FindFeatureDefIndex(int feature_id,FEATURETYPE *ret_type)
{
  if(ret_type==NULL)return -1;
  *ret_type=FEATURETYPE::NA;
  for(int i=0;i<featureCircleList.size();i++)
  {
    if(featureCircleList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::ARC;
      return i;
    }
  }

  
  for(int i=0;i<featureLineList.size();i++)
  {
    if(featureLineList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::LINE;
      return i;
    }
  }

  
  for(int i=0;i<judgeList.size();i++)
  {
    if(judgeList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::MEASURE;
      return i;
    }
  }

  for(int i=0;i<auxPointList.size();i++)
  {
    if(auxPointList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::AUX_POINT;
      return i;
    }
  }

  
  for(int i=0;i<searchPointList.size();i++)
  {
    if(searchPointList[i].id == feature_id)
    {
      *ret_type = FEATURETYPE::SEARCH_POINT;
      return i;
    }
  }
  return -1;
}


int FeatureManager_sig360_circle_line::FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report,int feature_id,FEATURETYPE *ret_type)
{
  if(ret_type==NULL)return -1;
  *ret_type=FEATURETYPE::NA;
  for(int i=0;i<report.detectedCircles->size();i++)
  {
    featureDef_circle *def = (*report.detectedCircles)[i].def;
    if(def==NULL)continue;
    if(def->id == feature_id)
    {
      *ret_type = FEATURETYPE::ARC;
      return i;
    }
  }

  for(int i=0;i<report.detectedLines->size();i++)
  {
    featureDef_line *def = (*report.detectedLines)[i].def;
    if(def==NULL)continue;
    if(def->id == feature_id)
    {
      *ret_type = FEATURETYPE::LINE;
      return i;
    }
  }
  
  for(int i=0;i<report.detectedAuxPoints->size();i++)
  {
    featureDef_auxPoint *def = (*report.detectedAuxPoints)[i].def;
    if(def==NULL)continue;
    if(def->id == feature_id)
    {
      *ret_type = FEATURETYPE::AUX_POINT;
      return i;
    }
  }
  
  for(int i=0;i<report.detectedSearchPoints->size();i++)
  {
    featureDef_searchPoint *def = (*report.detectedSearchPoints)[i].def;
    if(def==NULL)continue;
    if(def->id == feature_id)
    {
      *ret_type = FEATURETYPE::SEARCH_POINT;
      return i;
    }
  }

  for(int i=0;i<report.judgeReports->size();i++)
  {
    FeatureReport_judgeDef *def = (*report.judgeReports)[i].def;
    if(def==NULL)continue;
    if(def->id == feature_id)
    {
      *ret_type = FEATURETYPE::MEASURE;
      return i;
    }
  }

  return -1;
}

int FeatureManager_sig360_circle_line::ParseMainVector(float flip_f,FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *vec)
{
  if(vec == NULL)return -1;
  FEATURETYPE type;
  int idx = FindFeatureReportIndex(report, feature_id,&type);
  if(idx <0)return -1;
  switch(type)
  {

    case MEASURE:
    case ARC:
    case AUX_POINT:
      return -1;
    case LINE:
    {
      
      if( (*report.detectedLines)[idx].status ==  
        FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      acv_LineFit line = (*report.detectedLines)[idx].line;
      *vec = line.line.line_vec;
      return 0;
    }
    case SEARCH_POINT:
    {
      FeatureReport_searchPointReport sPoint = (*report.detectedSearchPoints)[idx];
      
      if( sPoint.status ==  FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      if(sPoint.def->subtype != featureDef_searchPoint::anglefollow)
        return -1;
      acv_XY line_vec;
      int ret_val = ParseMainVector(flip_f,report,sPoint.def->data.anglefollow.target_id, &line_vec);
      if(ret_val<0)return -1;
      float angle = sPoint.def->data.anglefollow.angleDeg*M_PI/180;
      if(flip_f<0)angle*=-1;
      acv_XY ret_vec =  acvRotation(sin(angle),cos(angle),1,line_vec);
      *vec=ret_vec;
      return 0;
    }
    break;
  }
  return -1;
}


int FeatureManager_sig360_circle_line::lineCrossPosition(float flip_f,FeatureReport_sig360_circle_line_single &report,int obj1_id,int obj2_id, acv_XY *pt)
{
  if(pt == NULL)return -1;
  FEATURETYPE type1=FEATURETYPE::NA,type2=FEATURETYPE::NA;

  acv_XY vec1,vec2;

  if(ParseMainVector(flip_f,report,obj1_id, &vec1) !=0 ||
    ParseMainVector(flip_f,report,obj2_id, &vec2) !=0 )
    {
      return -1;
    }

  acv_XY pt11,pt21;
  if(ParseLocatePosition(report,obj1_id, &pt11) !=0 ||
    ParseLocatePosition(report,obj2_id, &pt21) !=0 )
    {
      return -1;
    }

  acv_XY pt12 = acvVecAdd(vec1,pt11);
  acv_XY pt22 = acvVecAdd(vec2,pt21);


  acv_XY cross = acvIntersectPoint(pt11,pt12,pt21,pt22);
  
  *pt=cross;
  return 0;
}

int FeatureManager_sig360_circle_line::ParseLocatePosition(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *pt)
{
  if(pt == NULL)return -1;
  FEATURETYPE type;
  int idx = FindFeatureReportIndex(report,feature_id,&type);
  if(idx <0)return -1;
  switch(type)
  {

    case MEASURE:
      return -1;
    case ARC:
    {
      FeatureReport_circleReport cir = (*report.detectedCircles)[idx];
      
      if( cir.status ==  FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      *pt=cir.circle.circle.circumcenter;
      return 0;
    }

    case AUX_POINT:
    {
      FeatureReport_auxPointReport aPoint = (*report.detectedAuxPoints)[idx];
      if( aPoint.status ==  FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      *pt=aPoint.pt;
      return 0;
    }
    case LINE:
    {
      FeatureReport_lineReport line_report = (*report.detectedLines)[idx];
      if( line_report.status ==  FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      acv_LineFit line = line_report.line;
      *pt = line.line.line_anchor;
      return 0;
    }
    case SEARCH_POINT:
    {
      FeatureReport_searchPointReport sPoint = (*report.detectedSearchPoints)[idx];
      if( sPoint.status ==  FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        return -2;
      }
      
      *pt=sPoint.pt;
      return 0;
    }
    break;
  }
  return -1;
}


FeatureReport_judgeReport FeatureManager_sig360_circle_line::measure_process
  (FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
  FeatureReport_judgeDef &judge)
{

  //vector<FeatureReport_judgeReport> &judgeReport = *report.judgeReports;
  FeatureReport_judgeReport judgeReport={0};
  judgeReport.def = &judge;
  judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
  LOGV("judge:%s  OBJ1:%d, OBJ2:%d subtype:%d",judge.name,judge.OBJ1_id,judge.OBJ2_id,judge.measure_type);
  //LOGV("OBJ1_type:%d idx:%d   OBJ2_type:%d idx:%d ",judge.OBJ1_type,judge.OBJ1_idx,judge.OBJ2_type,judge.OBJ2_idx);
  LOGV("val:%f  USL:%f,LSL:%f",judge.targetVal,judge.USL,judge.LSL);

  FEATURETYPE type1=FEATURETYPE::NA,type2=FEATURETYPE::NA;
  int idx1 = FindFeatureReportIndex(report,judge.OBJ1_id, &type1);
  int idx2 = FindFeatureReportIndex(report,judge.OBJ2_id, &type2);
  if(idx1<0)return judgeReport;

  switch(judge.measure_type)
  {
    case FeatureReport_judgeDef::ANGLE:
      if(type1 != FEATURETYPE::LINE || type2 != FEATURETYPE::LINE)break;
      else{
        acv_XY vec1,vec2;

        if(ParseMainVector(flip_f,report,judge.OBJ1_id, &vec1) !=0 ||
          ParseMainVector(flip_f,report,judge.OBJ2_id, &vec2) !=0 )
          {
            break;
          }

        int quadrant = judge.data.ANGLE.quadrant;

        
        float sAngle = atan2(vec1.Y,vec1.X);
        float eAngle = atan2(vec2.Y,vec2.X);
        
        float angleDiff = (eAngle - sAngle);
        
        if(angleDiff>2*M_PI)angleDiff-=2*M_PI;
        else if(angleDiff<-2*M_PI)angleDiff+=2*M_PI;


        LOGV("quadrant:%d ",quadrant);
        LOGV("angleDiff:%f _________%f_%f_%f",angleDiff*180/M_PI,vec1.X,vec1.Y,atan2(vec1.Y,vec1.X)*180/M_PI);
        LOGV("angleDiff:%f _________%f_%f_%f",angleDiff*180/M_PI,vec2.X,vec2.Y,atan2(vec2.Y,vec2.X)*180/M_PI);
        if(angleDiff<0)//Find diff angle 0~2PI
        {
          angleDiff+=M_PI*2;
        }
        //The logic blow is to find angle in 4 quadrants (line1 as x axis, line2 as y axis)
        //So actually only 2 angles available ( quadrant 1 angle == quadrant 3 angle ) ( quadrant 2 angle == quadrant 4 angle )
        //quadrant 0 means quadrant 4 => quadrant n%2


        //If the angle is begger than PI(180) means the vector that we use to calculate angle is reversed
        //Like we use x axis and -y axis, the angle will be 270 instead of 90, so we will use 
        if(angleDiff>M_PI)
        {
          angleDiff-=M_PI;
        }
        //Now we have the quadrant 1 angle

        if( (quadrant%2==0) ^ (flip_f!=1) )//if our target quadrant is 2 or 4..., find the complement angle 
        {
          angleDiff=M_PI-angleDiff;
        }
        judgeReport.measured_val=180*angleDiff/M_PI;//Convert to degree

        //HACK: the current method would have 0<->180 jumping back&forward issue
        //So warp around if the diff value is too much
        float measureDiff=judgeReport.measured_val - judgeReport.def->targetVal;
        if(measureDiff<-90)
        {
          judgeReport.measured_val+=180;
        }
        else if(measureDiff>90)
        {
          judgeReport.measured_val-=180;
        }


        
        //angleDiff = judgeReport.measured_val - judgeReport.def->LSL;
        
        if(judgeReport.measured_val >judgeReport.def->USL || 
           judgeReport.measured_val <judgeReport.def->LSL)
        {
          judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
        }
        else
        {
          judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        }

      }

    break;
    case FeatureReport_judgeDef::DISTANCE :
    {
        int ret;
        acv_XY vec1,pt1,pt2;
        
        if(ParseLocatePosition(report,judge.OBJ1_id, &pt1) !=0 ||
          ParseLocatePosition(report,judge.OBJ2_id, &pt2) !=0 )
          {
            break;
          }
        int baseLine_id = judge.ref_baseLine_id;
        if(baseLine_id<0)baseLine_id=judge.OBJ1_id;
        ret = ParseMainVector(flip_f,report,baseLine_id, &vec1);
        if(ret!=0)
        {//OBJ1 have no direction
          judgeReport.measured_val=acvDistance(pt1,pt2);
        }
        else
        {
          acv_Line line;
          line.line_vec = vec1;
          line.line_anchor = pt1;
          pt1 = acvClosestPointOnLine(pt2,line);
          judgeReport.measured_val=acvDistance(pt1,pt2);

        }
        //float diff = judgeReport.measured_val - judgeReport.def->targetVal;
        //if(diff < 0)diff = -diff;
        if(judgeReport.measured_val >judgeReport.def->USL || 
           judgeReport.measured_val <judgeReport.def->LSL)
        {
          judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
        }
        else
        {
          judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        }
    }


    break;
    case FeatureReport_judgeDef::RADIUS :
    {
      if(type1 != FEATURETYPE::ARC)break;
      FeatureReport_circleReport cir = (*report.detectedCircles)[idx1];
      judgeReport.measured_val=cir.circle.circle.radius;
      
    
      if(judgeReport.measured_val >judgeReport.def->USL || 
        judgeReport.measured_val <judgeReport.def->LSL)
      {
        judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
      }
      else
      {
        judgeReport.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
      }
    }
    break;
    case FeatureReport_judgeDef::SIGMA:
    {
      judgeReport.measured_val=0;
    }
    break;
  }


  LOGV("===================");

  return judgeReport;
}


FeatureReport_auxPointReport FeatureManager_sig360_circle_line::auxPoint_process
  (FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
  featureDef_auxPoint &def)
{
  
    FeatureReport_auxPointReport rep;
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    rep.def = &def;
    switch(def.subtype)
    {
      case featureDef_auxPoint::lineCross:
      {
        acv_XY cross;
        int ret = lineCrossPosition(flip_f,report,
        def.data.lineCross.line1_id,
        def.data.lineCross.line2_id, &cross);
        if(ret<0)
        {
          return rep;
        }
        
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        rep.pt = cross;
        return rep;
      }
      
      case featureDef_auxPoint::centre :
      {
        if(this->signature_feature_id == def.data.centre.obj1_id)
        {
          rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
          rep.pt = report.Center;
          return rep;
        }
        acv_XY retXY;
        int ret_val = ParseLocatePosition(report,def.data.centre.obj1_id, &retXY);
        if(ret_val!=0)
        {
          return rep;
        }
        rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        rep.pt = retXY;
        return rep;
      }
    }
    return rep;
}



FeatureReport_searchPointReport FeatureManager_sig360_circle_line::searchPoint_process
  (acvImage *grayLevelImg,acvImage *labeledImg,int labelId,acv_LabeledData labeledData,FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,float thres,
  featureDef_searchPoint &def,acvImage *dbgImg)
{
    FeatureReport_searchPointReport rep;
    rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
    rep.def = &def;
    switch(def.subtype)
    {
      case featureDef_searchPoint::anglefollow:
      {
        acv_XY vec;
        int ret_val;

        {
              
          ret_val = ParseMainVector(flip_f,report,def.data.anglefollow.target_id, &vec);
          if(ret_val<0)
          {
            break;
          }
          float angle = def.data.anglefollow.angleDeg*M_PI/180;
          if(flip_f<0)
          {
            angle=-angle;//depends on flip or not invert the angle
          }
          
          LOGV("line vec:%f %f",vec.X,vec.Y);
          vec =  acvRotation(sin(angle),cos(angle),1,vec);//No need to do the flip rotation 
          LOGV("Angle:%f",angle*180/M_PI);
          LOGV("line vec:%f %f",vec.X,vec.Y);
        }

        acv_XY pt = acvRotation(sine,cosine,flip_f,def.data.anglefollow.position);//Rotate the default point
        pt=acvVecAdd(pt,labeledData.Center);
        
        float width = def.width;
        float margin = def.margin;

        vec=acvVecNormalize(vec);
        acv_XY searchVec_nor = vec;
        acv_XY searchVec =  acvVecNormal(vec);
        
        if(flip_f>0)
        {
          searchVec = acvVecMult(searchVec,-1);
        }
        LOGV("pt:%f %f",pt.X,pt.Y);
        LOGV("searchVec_nor:%f %f",searchVec_nor.X,searchVec_nor.Y);
        LOGV("searchVec:%f %f",searchVec.X,searchVec.Y);
        acv_XY searchStart = acvVecMult(searchVec,-margin);
        searchStart=acvVecAdd(searchStart,pt);
        
        if(0)
        {
          acvDrawLine(dbgImg,
            pt.X,
            pt.Y,
            pt.X+searchVec.X*margin*2,
            pt.Y+searchVec.Y*margin*2,
            20,255,128);


          acvDrawLine(dbgImg,
            pt.X+searchVec_nor.X*width/2,
            pt.Y+searchVec_nor.Y*width/2,
            pt.X-searchVec_nor.X*width/2,
            pt.Y-searchVec_nor.Y*width/2,
            20,255,128);

        }

        acv_XY searchVec_nor_edge = acvVecMult(searchVec_nor,-width/2);
        searchStart=acvVecAdd(searchStart,searchVec_nor_edge);

        LOGV("searchStart:%f %f",searchStart.X,searchStart.Y);
        acv_XY searchPt = searchStart;//Find from start line to the end line;
        acv_XY searchPt_sum={0};
        int foundC =0;
        
        LOGV("searchPt:%f %f",searchPt.X,searchPt.Y);
        float nearestDist=10000;
        acv_XY nearestPt;


        int stepX=1;
        acv_XY searchVec_norX = acvVecMult(searchVec_nor,stepX);
        int stepY=1;
        acv_XY searchVecY = acvVecMult(searchVec,stepY);


        for(int j=0;j<width;j+=stepX,searchPt=acvVecAdd(searchPt,searchVec_norX))
        {
          acv_XY curPt = searchPt;
          for(int i=0;i<margin*2;i+=stepY,curPt=acvVecAdd(curPt,searchVecY))
          {
            if(i>nearestDist+stepY+4)break;
            int Y = (int)round(curPt.Y);
            int X = (int)round(curPt.X);

            if(Y<0 || Y>=labeledImg->GetHeight() || X<0 || X>=labeledImg->GetWidth() )
            {
              continue;
            }
            
            //LOGV("X:%d Y:%d",X,Y);
            uint8_t *pix = &(labeledImg->CVector[Y][X*3]);
            

            if(pix[0]!=255)
            {
              _3BYTE *lableId = (_3BYTE*)pix;
              if(labelId == lableId->Num)
              {
                int b = grayLevelImg->CVector[Y][X*3];
                if(b<thres)
                {
                  acv_XY retPt;
                  float ret_rsp;
                  if(EdgePointOpt2(grayLevelImg,searchVec,curPt,3,thres,&retPt,&ret_rsp)==0)
                  //if(EdgePointOpt(grayLevelImg,searchVec,tmp_pt,&ret_point_opt,&edgeResponse)==0)
                  {
                    //EdgePointOpt2(grayLevelImg,searchVec,retPt,3,thres,&retPt,&ret_rsp);
                    float dist = acvDistance(retPt,searchPt);
                    if(nearestDist>dist)
                    {
                      nearestDist = dist;
                      nearestPt = retPt;
                    }



                    break;
                  }
          
                  continue;
                }
              }
            }
          }
        }



        if(nearestDist<10000)
        {
          float ret_rsp;
          LOGV("nearestPt:%f %f",nearestPt.X,nearestPt.Y); 
          rep.pt = acvVecRadialDistortionRemove(nearestPt,param);         
          rep.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        }
        else
        {
          rep.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
        }
        
        LOGV("found:%d,rep.pt:%f %f, status:%d",foundC,rep.pt.X,rep.pt.Y, rep.status);
      }
      break;
    }

    return rep;
}



int FeatureManager_sig360_circle_line::parse_searchPointData(cJSON * jobj)
{

  featureDef_searchPoint searchPoint;

  {
    char *tmpstr;
    searchPoint.name[0] = '\0';
    if(tmpstr = json_find_name(jobj))
    {
      strcpy(searchPoint.name,tmpstr);
    }
  }

  double *pnum;

  if((pnum=JSON_GET_NUM(jobj,"id")) == NULL )
  {
    LOGE("No id");
    return -1;
  }
  searchPoint.id = (int)*pnum;
  LOGV("feature is an searchPoint:%s %d",searchPoint.name, searchPoint.id);

  //The subtype might be determined by definition file 
  searchPoint.subtype = featureDef_searchPoint::anglefollow;

  
  searchPoint.margin=
    *JFetEx_NUMBER(jobj,"margin");
  
  searchPoint.width=
    *JFetEx_NUMBER(jobj,"width");

  switch(searchPoint.subtype)
  {
    case featureDef_searchPoint::anglefollow:
    {
      searchPoint.data.anglefollow.angleDeg=
       *JFetEx_NUMBER(jobj,"angleDeg");

      searchPoint.data.anglefollow.target_id=(int)
       *JFetEx_NUMBER(jobj,"ref[0].id");
      
      searchPoint.data.anglefollow.position.X=
       *JFetEx_NUMBER(jobj,"pt1.x");
      searchPoint.data.anglefollow.position.Y=
       *JFetEx_NUMBER(jobj,"pt1.y");


      LOGV("searchPoint.X:%f Y:%f angleDeg:%f tar_id:%d",
      searchPoint.data.anglefollow.position.X,
      searchPoint.data.anglefollow.position.Y,
      searchPoint.data.anglefollow.angleDeg,
      searchPoint.data.anglefollow.target_id
      );



    }
    break;
    default:
      return -1;
  }
  
  searchPointList.push_back(searchPoint);



  return 0;
}

int FeatureManager_sig360_circle_line::parse_auxPointData(cJSON * jobj)
{
  featureDef_auxPoint auxPoint;

  strcpy(auxPoint.name,JFetEx_STRING(jobj,"name"));
  double *pnum;

  auxPoint.id = (int)*JFetEx_NUMBER(jobj,"id");
  LOGV("feature is an auxPoint:%s %d",auxPoint.name, auxPoint.id);


  

  if(JFetch_OBJECT(jobj,"ref[1]")!=NULL)
  {
    //The subtype might be determined by definition file 
    auxPoint.subtype = featureDef_auxPoint::lineCross;
    auxPoint.data.lineCross.line1_id=(int)*JFetEx_NUMBER(jobj,"ref[0].id");
    auxPoint.data.lineCross.line2_id=(int)*JFetEx_NUMBER(jobj,"ref[1].id");

    LOGV("id_1:%d id_2:%d",
      auxPoint.data.lineCross.line1_id,auxPoint.data.lineCross.line2_id
    );
  }
  else
  {
    auxPoint.subtype = featureDef_auxPoint::centre;
    auxPoint.data.centre.obj1_id = (int)*JFetEx_NUMBER(jobj,"ref[0].id");
    LOGV("the auxPoint is centre for the obj1_id:%d ",
      auxPoint.data.centre.obj1_id);
  }
  auxPointList.push_back(auxPoint);
  return 0;
}
int FeatureManager_sig360_circle_line::parse_lineData(cJSON * line_obj)
{
  featureDef_line line;
  line.MatchingMarginX=0;
  double *pnum;

  {
    char *tmpstr;
    line.name[0] = '\0';
    if(tmpstr = json_find_name(line_obj))
    {
      strcpy(line.name,tmpstr);
    }
  }
  
  line.id=(int)*JFetEx_NUMBER(line_obj,"id");

  LOGV("feature is a line:%s %d",line.name,line.id);

  line.initMatchingMargin=(float)*JFetEx_NUMBER(line_obj,"margin");

  double direction = *JFetEx_NUMBER(line_obj,"direction");

  if((pnum=JSON_GET_NUM(line_obj,"MatchingMarginX")) == NULL )
  {
    LOGI("The MatchingMarginX isn't there will be generated later on...");
  }
  else
  {
    line.MatchingMarginX=*pnum;
  }


  /*cJSON *kspArr_obj=(cJSON *)JFetch(line_obj,"searchKeyPoints",cJSON_Array);
  if(kspArr_obj)
  {
    int ret = parse_search_key_points_Data(kspArr_obj,line.skpsList);
    line.MatchingMarginX=find_search_key_points_longest_distance(line.skpsList)/2;

    featureLineList.push_back(line);
    return 0;
  }*/




  acv_XY p0,p1;
  p0.X=*JFetEx_NUMBER(line_obj,"pt1.x");
  p0.Y=*JFetEx_NUMBER(line_obj,"pt1.y");
  p1.X=*JFetEx_NUMBER(line_obj,"pt2.x");
  p1.Y=*JFetEx_NUMBER(line_obj,"pt2.y");

  if(line.MatchingMarginX==0)
  {
    line.MatchingMarginX=hypot(p0.X-p1.X,p0.Y-p1.Y)/2;
  }



  line.lineTar.line_anchor.X=(p0.X+p1.X)/2;
  line.lineTar.line_anchor.Y=(p0.Y+p1.Y)/2;
  line.lineTar.line_vec.X=(p1.X-p0.X);
  line.lineTar.line_vec.Y=(p1.Y-p0.Y);
  line.lineTar.line_vec = acvVecNormalize(line.lineTar.line_vec);

  if(direction<0)
  {
    line.lineTar.line_vec = acvVecMult(line.lineTar.line_vec,-1);
  }


  acv_XY normal = acvVecNormal(line.lineTar.line_vec);

  line.searchVec=normal;
  line.searchDist=line.initMatchingMargin*2;
  

  line.searchEstAnchor = line.lineTar.line_anchor;
  line.searchEstAnchor.X-=normal.X*line.initMatchingMargin;
  line.searchEstAnchor.Y-=normal.Y*line.initMatchingMargin;

  int keyPointCount=2+3;

  {
    for(int i=0;i<keyPointCount;i++)
    {
      featureDef_line::searchKeyPoint skeypt={
        keyPt:{
          X:p0.X+i*(p1.X-p0.X)/(keyPointCount-1) - normal.X*line.initMatchingMargin,
          Y:p0.Y+i*(p1.Y-p0.Y)/(keyPointCount-1) - normal.Y*line.initMatchingMargin
        }
      };
      line.keyPtList.push_back(skeypt);
    }
  }

  LOGV("anchor.X:%f anchor.Y:%f vec.X:%f vec.Y:%f ,MatchingMargin:%f",
  line.lineTar.line_anchor.X,
  line.lineTar.line_anchor.Y,
  line.lineTar.line_vec.X,
  line.lineTar.line_vec.Y,
  line.initMatchingMargin);
  LOGV("searchVec X:%f Y:%f vX:%f vY:%f,searchDist:%f",
  line.searchEstAnchor.X,
  line.searchEstAnchor.Y,
  line.searchVec.X,
  line.searchVec.Y,
  line.searchDist);

  if(line.name[0]=='\0')
  {
    sprintf(line.name,"@LINE_%d",featureLineList.size());
  }


  featureLineList.push_back(line);

  return 0;
}

int FeatureManager_sig360_circle_line::parse_sign360(cJSON * signature_obj)
{

  if( feature_signature.size()!=0 )
  {
    LOGE("feature_signature:size()=%d is set already. There can only be one signature feature.",feature_signature.size());
    return -1;
  }

  this->signature_feature_id = (int)*JFetEx_NUMBER(signature_obj,"id");
  cJSON *signature;
  if(!(getDataFromJsonObj(signature_obj,"signature",(void**)&signature)&cJSON_Object))
  {
    LOGE("The signature is not an cJSON_Object");
    return -1;
  }

  cJSON *signature_magnitude;
  cJSON *signature_angle;
  if(!(getDataFromJsonObj(signature,"magnitude",(void**)&signature_magnitude)&cJSON_Array))
  {
    LOGE("The signature_magnitude is not an cJSON_Array");
    return -1;
  }

  if(!(getDataFromJsonObj(signature,"angle",(void**)&signature_angle)&cJSON_Array))
  {
    LOGE("The signature_angle is not an cJSON_Array");
    return -1;
  }

  if( cJSON_GetArraySize(signature_magnitude) != cJSON_GetArraySize(signature_angle) )
  {
    LOGE("The signature_angle and signature_magnitude doesn't have same length");
    return -1;
  }

  if( cJSON_GetArraySize(signature_magnitude) != 360 )
  {
    LOGE("The signature size(%d) is not equal to 360",cJSON_GetArraySize(signature_magnitude));
    return -1;
  }


  for (int i = 0 ; i < cJSON_GetArraySize(signature_magnitude) ; i++)
  {
    double *pnum_mag;
    if(!(getDataFromJsonObj(signature_magnitude,i,(void**)&pnum_mag)&cJSON_Number))
    {
      return -1;
    }

    double *pnum_ang;
    if(!(getDataFromJsonObj(signature_angle,i,(void**)&pnum_ang)&cJSON_Number))
    {
      return -1;
    }
    acv_XY dat={.X=(float)*pnum_mag,.Y=(float)*pnum_ang};

    feature_signature.push_back(dat);
    /*cJSON * feature = cJSON_GetArrayItem(signature, i);
    LOGI(" %f",type_str,ver_str,unit_str);*/
  }

  LOGV("feature is a signature");


  return 0;
}

int FeatureManager_sig360_circle_line::parse_judgeData(cJSON * judge_obj)
{

  FeatureReport_judgeDef judge={0};
  double *pnum;

  char *tmpstr=JFetEx_STRING(judge_obj,"name");
  strcpy(judge.name,tmpstr);

  judge.id = (int)*JFetEx_NUMBER(judge_obj,"id");

  
  char *subtype = JFetEx_STRING(judge_obj,"subtype");

  judge.measure_type=FeatureReport_judgeDef::NA;

  if(strcmp(subtype, "sigma")==0 )
  {
    judge.measure_type=FeatureReport_judgeDef::SIGMA;
  }
  else if(strcmp(subtype, "radius")==0 )
  {
    judge.measure_type=FeatureReport_judgeDef::RADIUS;
  }
  else if(strcmp(subtype, "distance")==0 )
  {
    judge.measure_type=FeatureReport_judgeDef::DISTANCE;
  }
  else if(strcmp(subtype, "angle")==0 )
  {
    judge.measure_type=FeatureReport_judgeDef::ANGLE;
    pnum=JFetch_NUMBER(judge_obj,"quadrant");
    if(pnum==NULL)
    {
      judge.data.ANGLE.quadrant = 0;
    }
    else
    {
      judge.data.ANGLE.quadrant = (int)*pnum;
    }
    LOGV("quadrant:%d",judge.data.ANGLE.quadrant);
  }
  else if(strcmp(subtype, "area")==0 )
  {
    judge.measure_type=FeatureReport_judgeDef::AREA;

  }

  if(judge.measure_type==FeatureReport_judgeDef::NA)
  {
    
    LOGV("the subtype:%s does not belong to what we support",subtype);
    LOGV("sigma,distance,angle,radius");
    return -1;
  }
  LOGV("feature is a measure/judge:%s id:%d subtype:%s",judge.name,judge.id,subtype);


  judge.targetVal=*JxNUM(judge_obj,"value");

  judge.USL=*JxNUM(judge_obj,"USL");
  judge.LSL=*JxNUM(judge_obj,"LSL");

  
  judge.OBJ1_id = (int)*JxNUM(judge_obj,"ref[0].id");

  pnum = JFetch_NUMBER(judge_obj,"ref[1].id");//It's fine if we don't have OBJ2(ref[1])
  if(pnum == NULL)judge.OBJ2_id = -1;
  else {judge.OBJ2_id = *pnum;}

  
  pnum = JFetch_NUMBER(judge_obj,"ref_baseLine.id");//It's fine if we don't have ref_baseLine
  if(pnum == NULL)judge.ref_baseLine_id = -1;
  else {judge.ref_baseLine_id = *pnum;}

  LOGV("value:%f USL:%f LSL:%f id1:%d id2:%d",judge.targetVal,judge.USL,judge.LSL,judge.OBJ1_id,judge.OBJ2_id);
  judgeList.push_back(judge);
  return 0;
}



int FeatureManager_sig360_circle_line::parse_jobj()
{
  const char *type_str= JFetch_STRING(root,"type");
  const char *ver_str = JFetch_STRING(root,"ver");
  const char *unit_str =JFetch_STRING(root,"unit");
  if(type_str==NULL||ver_str==NULL||unit_str==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  unit:<%p>",type_str,ver_str,unit_str);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  unit:<%s>",type_str,ver_str,unit_str);


  this->matching_angle_margin=M_PI;//+-PI => 2PI matching margin
  this->matching_angle_offset=0;//original signature init matching angle
  this->matching_face=0;//front and back facing

  {
    double *margin_deg =JFetch_NUMBER(root,"matching_angle_margin_deg");
    if(margin_deg!=NULL)
    {
      this->matching_angle_margin=*margin_deg*M_PI/180;
      double *angle_offset_deg =JFetch_NUMBER(root,"matching_angle_offset_deg");
      if(angle_offset_deg!=NULL)
      {
        this->matching_angle_offset=*angle_offset_deg*M_PI/180;
      }
    }
    double *face =JFetch_NUMBER(root,"matching_face");
    if(face!=NULL)
    {
      this->matching_face=(int)*face;
    }
  }

  cJSON *featureList = cJSON_GetObjectItem(root,"features");
  cJSON *inherentfeatureList = cJSON_GetObjectItem(root,"inherentfeatures");

  if(featureList==NULL)
  {
    LOGE("features array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureList))
  {
    LOGE("features is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureList) ; i++)
  {
     cJSON * feature = cJSON_GetArrayItem(featureList, i);
     const char *feature_type =JFetch_STRING(feature, "type");
     if(feature_type == NULL)
     {
       LOGE("feature[%d] has no type...",i);
       return -1;
     }

     if(strcmp(feature_type, "arc")==0)
     {
       if(parse_arcData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "line")==0)
     {
       if(parse_lineData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "aux_point")==0)
     {
       
       if(parse_auxPointData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "search_point")==0)
     {
       
       if(parse_searchPointData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "measure")==0)
     {
       if(parse_judgeData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else
     {
       LOGE("feature[%d] has unknown type:[%s]",i,feature_type);
       return -1;
     }
  }

  if(inherentfeatureList==NULL)
  {
    LOGE("inherentfeatures does not exists");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(inherentfeatureList) ; i++)
  {
     cJSON * feature = cJSON_GetArrayItem(inherentfeatureList, i);
     const char *feature_type =JFetch_STRING(feature, "type");
     if(feature_type == NULL)
     {
       LOGE("feature[%d] has no type...",i);
       return -1;
     }
     if(strcmp(feature_type, "sign360")==0)
     {
       if(parse_sign360(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "aux_point")==0)
     {
       if(parse_auxPointData(feature)!=0)
       {
         LOGE("feature[%d] has error %s format",i,feature_type);
         return -1;
       }
     }
     else if(strcmp(feature_type, "aux_line")==0)
     {
       //return -1;
     }
     else
     {
       LOGE("feature[%d] has unknown type:[%s]",i,feature_type);
       return -1;
     }
  }


  if(0){
    //It's in parsing stage, there is no cameraParam yet.
    float ppmm =param.ppb2b/param.mmpb2b;//pixel 2 mm
    LOGV("_________  %f %f ",param.ppb2b,param.mmpb2b);
    //Convert mm to Pixel unit
    for(int i=0;i<featureLineList.size();i++)
    {
      featureLineList[i].searchDist*=ppmm;
      featureLineList[i].initMatchingMargin*=ppmm;
      featureLineList[i].MatchingMarginX*=ppmm;
      
      featureLineList[i].searchEstAnchor=acvVecMult(featureLineList[i].searchEstAnchor,ppmm);
      //featureLineList[i].searchVec=acvVecMult(featureLineList[i].searchVec,ppmm);

      vector<featureDef_line::searchKeyPoint> &keyPtList = featureLineList[i].keyPtList;
      for(int j=0;j<keyPtList.size();j++)
      {
        keyPtList[j].keyPt = acvVecMult(keyPtList[j].keyPt,ppmm);
      }
    }

    for(int i=0;i<featureCircleList.size();i++)
    {
      featureCircleList[i].outter_inner *=ppmm;
      featureCircleList[i].initMatchingMargin*=ppmm;

      featureCircleList[i].circleTar.circumcenter = 
        acvVecMult(featureCircleList[i].circleTar.circumcenter ,ppmm);
      featureCircleList[i].circleTar.radius*=ppmm;
    }

    for(int i=0;i<judgeList.size();i++)
    {
      judgeList[i].targetVal*=ppmm;
      judgeList[i].LSL*=ppmm;
      judgeList[i].USL*=ppmm;
    }
    
    for(int i=0;i<auxPointList.size();i++)
    {
      //No unit data to convert
      //auxPointList[i].data.lineCross.
    }

    for(int i=0;i<searchPointList.size();i++)
    {
      searchPointList[i].margin*=ppmm;
      searchPointList[i].width*=ppmm;
      if(searchPointList[i].subtype == featureDef_searchPoint::anglefollow)
      {
        searchPointList[i].data.anglefollow.position=
          acvVecMult(searchPointList[i].data.anglefollow.position,ppmm);
      }
    }


    for(int i=0;i<feature_signature.size();i++)
    {
      feature_signature[i].X*=ppmm;
    }

  }


  {
    for(int i=0;i<featureLineList.size();i++)
    {
      LOGV("featureLineList[%d]:%s",i,featureLineList[i].name);
    }
    for(int i=0;i<featureCircleList.size();i++)
    {
      LOGV("featureCircleList[%d]:%s",i,featureCircleList[i].name);
    }
    /*for(int i=0;i<judgeList.size();i++)
    {
      LOGV("judgeList[%d]:%s  OBJ1:%s, OBJ2:%s type:%d",i,judgeList[i].name,judgeList[i].OBJ1,judgeList[i].OBJ2,judgeList[i].measure_type);
      LOGV("-val:%f  margin:%f",judgeList[i].targetVal,judgeList[i].targetVal_margin);
    }*/
  }


  if(feature_signature.size()==0)
  {
    LOGE("No signature data");
    return -1;
  }

  return 0;
}


int FeatureManager_sig360_circle_line::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  featureCircleList.resize(0);
  featureLineList.resize(0);
  judgeList.resize(0);

  auxPointList.resize(0);
  searchPointList.resize(0);
  feature_signature.resize(0);



  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    featureCircleList.resize(0);
    featureLineList.resize(0);
    feature_signature.resize(0);
    reload("");
    return -2;
  }
  return 0;
}

int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist)
{
  if(img ==NULL || pos==NULL )return -1;
  int X=(int)round(pos->X);
  int Y=(int)round(pos->Y);
  if(X<0 || Y<0 || X>=img->GetWidth() || Y>=img->GetHeight())
  {
    return -1;
  }

  searchVec = acvVecNormalize(searchVec);

  int tarX=0;
  if(img->CVector[Y][3*X]==255)
  {
    tarX=0;//Looking for non-255
  }
  else
  {
    /*tarX=255;//Looking for 255
    searchVec.X*=-1;//reverse search vector
    searchVec.Y*=-1;*/
    return -1;
  }

  for(int i=0;i<maxSearchDist;i++)
  {
    X=(int)round(pos->X+searchVec.X*i);
    Y=(int)round(pos->Y+searchVec.Y*i);
    if(X<0 || Y<0 || X>=img->GetWidth() || Y>=img->GetHeight())
    {
      return -1;
    }

    if(img->CVector[Y][3*X]==255)
    {
      if(tarX == 255)
      {
        pos->X=pos->X+searchVec.X*(i-1);//Get previous non-255
        pos->Y=pos->Y+searchVec.Y*(i-1);
        return 0;
      }
    }
    else
    {
      if(tarX != 255)
      {
        pos->X=pos->X+searchVec.X*(i);
        pos->Y=pos->Y+searchVec.Y*(i);
        return 0;
      }
    }

  }

  return -1;
}


const FeatureReport* FeatureManager_sig360_circle_line::GetReport()
{
  report.type = FeatureReport::sig360_circle_line;
  //report.error = FeatureReport_sig360_circle_line::NONE;
  report.data.sig360_circle_line.reports = &reports;
  return &report;
}

void FeatureManager_sig360_circle_line::ClearReport()
{
  report.type = FeatureReport::sig360_circle_line;
  report.data.sig360_circle_line.error = FeatureReport_ERROR::NONE;
  reports.resize(0);
  report.data.sig360_circle_line.reports = &reports;
}

void spline9X(float *f,float *x,int fL,float *s,float *h)
{
    int n,i,j,k;
    const int L = 9;
    float a,b,c,d,sum,F[L],p,m[L][L]={0},temp;
    n = fL;


    for(i=n-1;i>0;i--)
    {
        F[i]=(f[i]-f[i-1])/(x[i]-x[i-1]);
        h[i-1]=x[i]-x[i-1];
    }
    
    //*********** formation of h, s , f matrix **************//
    for(i=1;i<n-1;i++)
    {
        m[i][i]=2*(h[i-1]+h[i]);
        if(i!=1)
        {
            m[i][i-1]=h[i-1];
            m[i-1][i]=h[i-1];
        }
        m[i][n-1]=6*(F[i+1]-F[i]);
    }

    /*
    for(i=0;i<n;i++)
    {
        printf(" %f",f[i]);
    }
    printf("....%d\n",n);
    
    for(i=0;i<n;i++)
    {
      for(j=0;j<n;j++)
      {
        printf(" %f",m[i][j]);
      }
      printf("\n");
    }*/
    

    //***********  forward elimination **************//

    for(i=1;i<n-2;i++)
    {
        temp=(m[i+1][i]/m[i][i]);
        for(j=1;j<=n-1;j++)
        {
            m[i+1][j]-=temp*m[i][j];
        }
    }

    //*********** back ward substitution *********//
    for(i=n-2;i>0;i--)
    {
        sum=0;
        for(j=i;j<=n-2;j++)
            sum+=m[i][j]*s[j];
        s[i]=(m[i][n-1]-sum)/m[i][i];
    }
}
void spline9_edge(float *f,int fL,float *edgeX,float *ret_edge_response)
{
    int n,i,j,k;
    const int L = 9;
    float h[L],a,b,c,d,s[L]={0},x[L];
    n = fL;

    for(i=0;i<n;i++)
    {
        x[i]=i;
        //printf("%f\n",f[i]);
    }

    spline9X(f,x,fL,s,h);



    float maxEdge_response = 0;
    float maxEdge_offset=NAN;
    
    float edgePowerInt = 0;
    float edgeCentralInt = 0;
    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        b=s[i]/2;
        c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
        d=f[i];
        
        //f' = 3ax^2+2bx + c
        //pow(f',2) = 9aax^4 + 12abx^3 + (6ac+4bb)x^2 + 4cbx +cc
        float edgePower = 
          (9*a*a)/5 + 
          (12*a*b)/4 + 
          (6*a*c+4*b*b)/3 +
          (4*c*b)/2+
          (c*c)/1;//Integeral(pow(f',2),0,1)

    
        float edgeCentral = 
          (9*a*a)/6 + 
          (12*a*b)/5 + 
          (6*a*c+4*b*b)/4 +
          (4*c*b)/3+
          (c*c)/2;//Integeral(pow(f'(x),2)x,0,1)
        edgeCentral/=edgePower;
        edgeCentralInt+=edgePower*(i+edgeCentral);

        edgePowerInt+=edgePower;
        bool zeroCross = (s[i+1]*s[i])<0;

        if(zeroCross || (s[i]==0 && i!=0))
        {
            float offset = s[i]/(s[i]-s[i+1]);
            float xi =offset;
            float firDir = 3*a*xi*xi+2*b*xi+c;
            float secDir = 6*a*xi+2*b;
            float abs_firDir = abs(firDir);
            if(maxEdge_response<abs_firDir)
            {
                maxEdge_response=abs_firDir;
                maxEdge_offset = i+offset;
            }
            //printf("cross: offset:%f\n",i+offset);
        }
    }



    float edgeSide1 = 0;
    float edgeSide2 = 0;
    for(i=0;i<n;i++)
    {
      if(i<maxEdge_offset)
      {
        edgeSide1+=f[i];
      }
      else
      {
        edgeSide2+=f[i];
      }
    }
    edgeSide1/=(1+(int)maxEdge_offset);
    edgeSide2/=(n-1-(int)maxEdge_offset);
    float edgeSideDiff = edgeSide1-edgeSide2;

    if(edgeSideDiff<0)edgeSideDiff=-edgeSideDiff;
    //edgeSideDiff=edgeSideDiff*edgeSideDiff;
    edgeSideDiff-=20;
    if(edgeSideDiff<0)edgeSideDiff=0;

    edgeCentralInt/=edgePowerInt;
    float edgeRange=1;
    float BGEdgePower = (edgePowerInt-maxEdge_response*maxEdge_response*edgeRange)/(n-1-edgeRange);
    if(BGEdgePower<0)BGEdgePower=0;
    BGEdgePower=sqrt(BGEdgePower);
    float SNR = maxEdge_response/(BGEdgePower+0.1);
    //printf("MAX rsp>>> %f %f %f\n",maxEdge_response,BGEdgePower, maxEdge_response/(BGEdgePower+0.1));
    
    //LOGV("%f %f %f %f %f %f %f --- %f:",f[0],f[1],f[2],f[3],f[4],f[5],f[6],edgeSideDiff);
    //LOGV("edgeCentralInt::%f %f",edgeCentralInt,maxEdge_offset);
    if(0)
    {
      *edgeX = edgeCentralInt;
      *ret_edge_response = edgePowerInt;
    }
    else
    {
      *edgeX = edgeCentralInt;
      *ret_edge_response = edgeSideDiff;
    }

}
void spline9_max(float *f,int fL,int div,float *ret_maxf,float *ret_maxf_x)
{
    int n,i,j,k;
    const int L = 9;
    float h[L],a,b,c,d,s[L]={0},x[L];
    n = fL;

    for(i=0;i<n;i++)
    {
        x[i]=i;
        //printf("%f\n",f[i]);
    }

    spline9X(f,x,fL,s,h);


    
    float maxf=f[0];
    float maxf_x=0;
    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        b=s[i]/2;
        c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
        d=f[i];
        //f = ax^3 + bx^2 + cx + d
        //f' = 3ax^2+2bx + c
        //Find max f = 
        if(maxf<f[i])
        {
          maxf=f[i];
          maxf_x=i;
        }
        for(j=1;j<div;j++)
        {
          float x_=(float)j/div;

          float val = a*x_*x_*x_ + b*x_*x_ + c*x_ +d;

          if(maxf<val)
          {
            maxf=val;
            maxf_x=i+x_;
          }
        }

    }
    *ret_maxf_x = maxf_x;
    *ret_maxf = maxf;

}
float OTSU_Threshold(acvImage &graylevelImg,acv_LabeledData *ldata,int skip=5)
     /* binarization by Otsu's method 
	based on maximization of inter-class variance */
     
{
  const int GRAYLEVEL=256;
  int hist[GRAYLEVEL];
  float prob[GRAYLEVEL], omega[GRAYLEVEL]; /* prob of graylevels */
  float myu[GRAYLEVEL];   /* mean value for separation */
  float max_sigma, sigma[GRAYLEVEL]; /* inter-class variance */
  int i, x, y; /* Loop variable */
  int threshold; /* threshold for binarization */
  
  
  /* Histogram generation */
  for (i = 0; i < GRAYLEVEL; i++) hist[i] = 0;
  int count =0;
  for (y = ldata->LTBound.Y; y < ldata->RBBound.Y; y+=skip)
    for (x = ldata->LTBound.X; x < ldata->RBBound.X; x+=skip) {
      hist[graylevelImg.CVector[y][3*x]]++;
      count++;
    }
  /* calculation of probability density */
  int totalPix = count;
  for ( i = 0; i < GRAYLEVEL; i ++ ) {
    prob[i] = (double)hist[i] /totalPix;
  }
  
  /* omega & myu generation */
  omega[0] = prob[0];
  myu[0] = 0.0;       /* 0.0 times prob[0] equals zero */
  for (i = 1; i < GRAYLEVEL; i++) {
    omega[i] = omega[i-1] + prob[i];
    myu[i] = myu[i-1] + i*prob[i];
  }
  
  /* sigma maximization
     sigma stands for inter-class variance 
     and determines optimal threshold value */
  threshold = 0;
  max_sigma = 0.0;
  for (i = 0; i < GRAYLEVEL-1; i++) {
    if (omega[i] != 0.0 && omega[i] != 1.0)
      sigma[i] = pow(myu[GRAYLEVEL-1]*omega[i] - myu[i], 2) / 
	(omega[i]*(1.0 - omega[i]));
    else
      sigma[i] = 0.0;
    if (sigma[i] > max_sigma) {
      max_sigma = sigma[i];
      threshold = i;

    }


    {
      //printf("ELSE Update %d %f\n",i,sigma[i]);
    }
  }
  
  int searchRange=7;
  float sigmaBaseIdx =threshold-(searchRange-1)/2; 
  float retMaxF=0;
  float retMaxF_X=0;
  spline9_max(&(sigma[(int)sigmaBaseIdx]),searchRange,10,&retMaxF,&retMaxF_X);
  sigmaBaseIdx+=retMaxF_X;
  //printf("\nthreshold value =s:%f i:%d  %f %f %f\n",max_sigma, threshold,retMaxF,sigmaBaseIdx,retMaxF_X);
  
  return sigmaBaseIdx;
}
int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,acv_XY *ret_point_opt,float *ret_edge_response)
{
  if(ret_point_opt==NULL)return -1;
  
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=7;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);

  
  const int nM=1;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  acv_XY nvecBM = acvVecMult(nvec,-(float)(nM-1)/2);
  
  //gradVec= acvVecMult(gradVec,1);
  
  acv_XY  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  acv_XY bkpoint = curpoint;
  curpoint = acvVecAdd(curpoint,nvecBM);
  for(int i=0;i<GradTableL;i++)
  {
    float ptn = 0;
    acv_XY tmpCurPt=curpoint;
    for(int j=0;j<nM;j++)
    {
      ptn+= acvUnsignedMap1Sampling(graylevelImg, tmpCurPt, 0);
      tmpCurPt = acvVecAdd(tmpCurPt,nvec);
    }
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn/nM;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  float edgeX;
  spline9_edge(gradTable,GradTableL,&edgeX,ret_edge_response);


  
  /*for(int i=0;i<GradTableL;i++)
  {
    printf("%5.2f ",gradTable[i]);
    //gradTable[i]=0;
  }
  printf("...edgeX:%f ret_edge_rsp:%f\n",edgeX,*ret_edge_response);
  */
  //LOGV("<<%f",edgeX);
  if(edgeX!=edgeX)//NAN
  {
    return -1;
  }
  /*
  LOGV("%f %f %f %f %f %f %f <= %f",
  gradTable[0],
  gradTable[1],
  gradTable[2],
  gradTable[3],
  gradTable[4],
  gradTable[5],
  gradTable[6],
  edgeX
  );*/
  gradVec = acvVecMult(gradVec,edgeX);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);


  return 0;
}



int EdgePointOpt2(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int range,float thres,acv_XY *ret_point_opt,float *ret_edge_response)
{
  if(ret_point_opt==NULL)return -1;
  
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=9;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);

  
  const int nM=range;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  acv_XY nvecBM = acvVecMult(nvec,-(float)(nM-1)/2);
  
  //gradVec= acvVecMult(gradVec,1);
  
  acv_XY  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  acv_XY bkpoint = curpoint;
  curpoint = acvVecAdd(curpoint,nvecBM);
  for(int i=0;i<GradTableL;i++)
  {
    float ptn = 0;
    acv_XY tmpCurPt=curpoint;
    for(int j=0;j<nM;j++)
    {
      ptn+= acvUnsignedMap1Sampling(graylevelImg, tmpCurPt, 0);
      tmpCurPt = acvVecAdd(tmpCurPt,nvec);
    }
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn/nM;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  /*
  for(int i=0;i<GradTableL;i++)
  {
    gradTable[i]-=thres;
  }
  for(int i=0;i<GradTableL-1;i++)
  {
    if(gradTable[i]*gradTable[i+1]<0)
    {
      if(gradTable[i]<0)
      {
        gradTable[i]*=-1;
        gradTable[i+1]*=-1;
      }
      
      float edgeX = i+gradTable[i]/(gradTable[i]-gradTable[i+1]);
      *ret_edge_response=10;
      
      gradVec = acvVecMult(gradVec,edgeX);
      *ret_point_opt = acvVecAdd(bkpoint,gradVec);

      return 0;
    }
  }*/

  float edgeWSum=0;
  float edgePos=0;
  for(int i=0;i<GradTableL;i++)
  {
    
    float diff = gradTable[i]-thres;
    if(diff<0)diff=-diff;
    //if(diff>10)continue;
    float weight = 1/(diff*diff/100+1);
    //printf("%g ",weight);
    edgePos+=i*weight;
    edgeWSum+=weight;
  }
  //LOGV("edgeWSum:%f",edgeWSum);

  if(edgeWSum==0)return -1;
  edgePos/=edgeWSum;

  int edgeMargin=(int)edgePos+1;
  int edgeMarginS=0;
  if((edgeMargin)*2>GradTableL)
  {
    edgeMargin=GradTableL -(edgeMargin);
    edgeMarginS = GradTableL - (edgeMargin)*2;
  }


  edgeWSum=0;
  edgePos=0;
  
  for(int i=edgeMarginS;i<edgeMarginS+(edgeMargin)*2;i++)
  {
    
    float diff = gradTable[i]-thres;
    if(diff<0)diff=-diff;
    //if(diff>10)continue;
    float weight = 1/(diff*diff/100+1);
    //printf("%g ",weight);
    edgePos+=i*weight;
    edgeWSum+=weight;
  }
  edgePos/=edgeWSum;
  

  if(ret_edge_response)
  {

    int avgPart1=0;
    int avgPart2=0;

    {
      edgeMargin=(int)edgePos+1;
      edgeMarginS=0;
      if((edgeMargin)*2>GradTableL)
      {
        edgeMargin=GradTableL -(edgeMargin);
        edgeMarginS = GradTableL - (edgeMargin)*2;
      }

      if(edgeMargin>3)edgeMargin=3;//we just want to calculate the slop, so don't make it to wide
      for(int i=0;i<(edgeMargin);i++)
      {
        avgPart1+=gradTable[edgeMarginS+edgeMargin-i-1];
        avgPart2+=gradTable[edgeMarginS+edgeMargin+i];
      }
    }

    avgPart1-=avgPart2;
    if(avgPart1<0)avgPart1=-avgPart1;

    *ret_edge_response=((float)avgPart1)/(edgeMargin+1);
  }

  gradVec = acvVecMult(gradVec,edgePos);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);



  return 0;
}

int EdgeGradientAdd(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,vector<ContourGrid::ptInfo> ptList,int width)
{
  const int GradTableL=7;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);
  //gradVec= acvVecMult(gradVec,1);
  
  acv_XY  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  ContourGrid::ptInfo tmp_pt;
  for(int i=0;i<GradTableL;i++)
  {
    float ptn= acvUnsignedMap1Sampling(graylevelImg, curpoint, 0);
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn;
    tmp_pt.pt=curpoint;
    curpoint = acvVecAdd(curpoint,gradVec);
  }

  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  for(int i=0;i<GradTableL-1;i++)
  {
    float diff = gradTable[i]-gradTable[i+1];
    if(diff<0)diff=-diff;

    tmp_pt.pt=curpoint;
    tmp_pt.edgeRsp = diff*diff*diff*diff*diff;
    ptList.push_back(tmp_pt);
    curpoint = acvVecAdd(curpoint,gradVec);
  }


  
  return 0;
}


int FeatureManager_sig360_circle_line::FeatureMatching(acvImage *img)
{
  float ppmm =param.ppb2b/param.mmpb2b;//pixel per mm
  acvImage *buff_=&_buff;
  vector<acv_LabeledData> &ldData=*this->_ldData;
  int grid_size = 50;
  bool drawDBG_IMG = false;
  buff_->ReSize(img->GetWidth(),img->GetHeight());
  buff1.ReSize(img->GetWidth(),img->GetHeight());
  buff2.ReSize(img->GetWidth(),img->GetHeight());
  acvImage *labeledBuff = &buff1;
  acvImage *smoothedImg = &buff2;
  //acvCloneImage( img,buff_, -1);
  acvCloneImage( img,labeledBuff, -1);
  acvCloneImage( originalImage,smoothedImg, -1);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,7);
  //acvBoxFilter(buff_,smoothedImg,2);
  //acvBoxFilter(buff_,smoothedImg,2);


  float feature_signature_ave=0;
  for(int i=0;i<feature_signature.size();i++)
  {
    feature_signature_ave+=feature_signature[i].X;
  }
  feature_signature_ave/=feature_signature.size();

  tmp_signature.resize(feature_signature.size());
  reports.resize(0);
  int scanline_skip=15;
  

  float sigma;
  int count = 0;

  {
      if(reportDataPool.size()<ldData.size())
      {
        int oriSize = reportDataPool.size();
        reportDataPool.resize(ldData.size());

        for(int i=oriSize;i<reportDataPool.size();i++)
        {
          reportDataPool[i].detectedCircles = new vector<FeatureReport_circleReport>(0);
          reportDataPool[i].detectedLines = new vector<FeatureReport_lineReport>(0);
          reportDataPool[i].detectedAuxPoints = new vector<FeatureReport_auxPointReport>(0);
          reportDataPool[i].detectedSearchPoints = new vector<FeatureReport_searchPointReport>(0);
          reportDataPool[i].judgeReports = new vector<FeatureReport_judgeReport>(0);
        }
      }
  }


  LOGV("ldData.size()=%d",ldData.size());
  for (int i = 2; i < ldData.size(); i++,count++)
  {// idx 0 is not a label, idx 1 is the outer frame to detect external object intrusion 
      if(ldData[i].area<120)continue;

      //LOGI("Lable:%2d area:%d",i,ldData[i].area);


      acvContourCircleSignature(img, ldData[i], i, tmp_signature);

      //the tmp_signature is in Pixel unit, convert it to mm
      for(int j=0;j<tmp_signature.size();j++)
      {
        tmp_signature[j].X/=ppmm;
      }

      bool isInv;
      float angle;
      
      float error = SignatureMinMatching( tmp_signature,feature_signature,
        //M_PI/2,M_PI/10,-1,
        //M_PI/2,M_PI*1.01/4,-1,
        this->matching_angle_offset,this->matching_angle_margin,this->matching_face,
        &isInv, &angle);

      error = sqrt(error)/feature_signature_ave;
      //if(i<10)
      {
        LOGV("======%d===X:%0.4f Y:%0.4f er:%f,inv:%d,angDeg:%f",i,ldData[i].Center.X,ldData[i].Center.Y,error,isInv,angle*180/3.14159);
      }


      if(error>0.2)continue;
      float mmpp = param.mmpb2b/param.ppb2b;//mm per pixel
      FeatureReport_sig360_circle_line_single singleReport=
      {
          .detectedCircles=reportDataPool[count].detectedCircles,
          .detectedLines=reportDataPool[count].detectedLines,
          .detectedAuxPoints=reportDataPool[count].detectedAuxPoints,
          .detectedSearchPoints=reportDataPool[count].detectedSearchPoints,
          .judgeReports = reportDataPool[count].judgeReports,
          .LTBound=ldData[i].LTBound,
          .RBBound=ldData[i].RBBound,
          .Center=ldData[i].Center,
          .area=(float)ldData[i].area,
          .rotate=angle,
          .isFlipped=isInv,
          .scale=1,
          .targetName=NULL,
      };

      singleReport.Center=acvVecMult(singleReport.Center,mmpp);
      singleReport.LTBound=acvVecMult(singleReport.LTBound,mmpp);
      singleReport.RBBound=acvVecMult(singleReport.RBBound,mmpp);
      singleReport.area*=mmpp*mmpp;



      //LOGV("======%d===er:%f,inv:%d,angDeg:%f",i,error,isInv,angle*180/3.14159);
      reports.push_back(singleReport);

      vector<FeatureReport_circleReport> &detectedCircles = *singleReport.detectedCircles;
      vector<FeatureReport_lineReport> &detectedLines = *singleReport.detectedLines;

      vector<FeatureReport_auxPointReport> &detectedAuxPoints = *singleReport.detectedAuxPoints;
      vector<FeatureReport_searchPointReport> &detectedSearchPoints = *singleReport.detectedSearchPoints;
      vector<FeatureReport_judgeReport> &judgeReports = *singleReport.judgeReports;

      detectedCircles.resize(0);
      detectedLines.resize(0);
      detectedAuxPoints.resize(0);
      detectedSearchPoints.resize(0);
      judgeReports.resize(0);

      float cached_cos,cached_sin;
      //The angle we get from matching is current object rotates 'angle' to match target
      //But now, we want to rotate feature set to match current object, so opposite direction
      angle=-angle;
      cached_cos=cos(angle);
      cached_sin=sin(angle);
      float flip_f=1;
      if(isInv)
      {
        flip_f=-1;
      }


      edge_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());
      
      acvRadialDistortionParam param=this->param;
      
      float thres = OTSU_Threshold(*smoothedImg,&ldData[i],3);
      LOGV("OTSU_Threshold:%f",thres);

      extractLabeledContourDataToContourGrid(smoothedImg,labeledBuff,i,ldData[i],
        grid_size,edge_grid,scanline_skip,param);
      
      
      if(drawDBG_IMG)//Draw debug image(curve and straight line)
      {
        for(int j=0;j<edge_grid.dataSize();i++)
        {
          const ContourGrid::ptInfo pti= *edge_grid.get(j);
          const acv_XY p = pti.pt;
          int X = round(p.X);
          int Y = round(p.Y);

          if(abs(pti.curvature)<0.05)
          {
            buff_->CVector[Y][X*3]=0;
            buff_->CVector[Y][X*3+1]=100;
            buff_->CVector[Y][X*3+2]=255;
          }
          else if(abs(pti.curvature)>0.06 && abs(pti.curvature)<0.4)
          {
            buff_->CVector[Y][X*3]=255;
            buff_->CVector[Y][X*3+1]=100;
            buff_->CVector[Y][X*3+2]=0;
          }
        }
      }


      acv_LineFit lf_zero = {0};
      for (int j = 0; j < featureLineList.size(); j++)
      {
        
        int mult=100;
        featureDef_line *line = &featureLineList[j];

        acv_XY target_vec = {0};
        acv_Line line_cand;

         
        {
          //line.lineTar.line_anchor = acvRotation(cached_sin,cached_cos,flip_f,line.lineTar.line_anchor);
          
          line_cand.line_vec = acvRotation(cached_sin,cached_cos,flip_f,line->lineTar.line_vec);
          line_cand.line_vec = acvVecMult(line_cand.line_vec,ppmm);//convert to pixel unit
          
            
          target_vec = line_cand.line_vec;
          line_cand.line_anchor =acvRotation(cached_sin,cached_cos,flip_f,line->searchEstAnchor);
          line_cand.line_anchor = acvVecMult(line_cand.line_anchor,ppmm);//convert to pixel unit
          
          //Offet to real image and backoff searchDist distance along with the searchVec as start
          line_cand.line_anchor=acvVecAdd(line_cand.line_anchor,ldData[i].Center);
          LOGV("line[%d]->lineTar.line_vec: %f %f",j,line_cand.line_vec.X,line_cand.line_vec.Y);
          LOGV("line[%d]->lineTar.line_anchor: %f %f",j,line_cand.line_anchor.X,line_cand.line_anchor.Y);
          
          
          acv_XY searchVec;
          searchVec = acvRotation(cached_sin,cached_cos,flip_f,line->searchVec);

          s_points.resize(0);
          for(int k=0;k<line->keyPtList.size();k++)
          {
            featureDef_line::searchKeyPoint skp= line->keyPtList[k];

            skp.keyPt= acvRotation(cached_sin,cached_cos,flip_f,skp.keyPt);
            skp.keyPt=acvVecMult(skp.keyPt,ppmm);
            skp.keyPt=acvVecAdd(skp.keyPt,ldData[i].Center);
            
            float searchDist = line->searchDist*ppmm;
            if(drawDBG_IMG)
            {
              acvDrawCrossX(buff_,
              skp.keyPt.X,skp.keyPt.Y,
              2,2);
            }
            //LOGV("skp.keyPt %f %f, searchDist:%f ppmm:%f",skp.keyPt.X,skp.keyPt.Y,searchDist,ppmm);
            if(searchP(img, &skp.keyPt , searchVec, searchDist)!=0)
            {
              LOGV("Fail...");
              continue;
            }
            if(drawDBG_IMG)
            {
              acvDrawCrossX(buff_,
              skp.keyPt.X,skp.keyPt.Y,
              2,4);
            }
            
                  
            //LOGV("skp.keyPt %f %f <= found..",skp.keyPt.X,skp.keyPt.Y);
            ContourGrid::ptInfo pti= {pt:skp.keyPt};

            s_points.push_back(pti);
          }
          if(s_points.size()>2)
          {
            //Use founded points to fit a candidate line
            acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, 
              s_points.size(),&line_cand,&sigma);
            
            LOGV(" %f %f, %f %f",line_cand.line_vec.X,line_cand.line_vec.Y,target_vec.X,target_vec.Y);
            LOGV("line_anchor: %f %f",line_cand.line_anchor.X,line_cand.line_anchor.Y);
            if( acv2DDotProduct(line_cand.line_vec,target_vec)<0 )
            {
              line_cand.line_vec = acvVecMult(line_cand.line_vec,-1);
            }
          }
          else if(s_points.size()==1)
          {
            line_cand.line_anchor = s_points[0].pt;
          }
          else
          {
            FeatureReport_lineReport lr;
            lr.line=lf_zero;
            lr.def=line;
            lr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;//Not able to find starting point
            detectedLines.push_back(lr);
            continue;
          }

        }
        // Apply distortion remove on init line candidate anchor(line 2335 Sig360_cir_lin..)
        // The init line candidate based on labeled image point scanning(5 init dots) and the labeled img is pre-distortion removal
        // Also, the point info in Contour grid are post-distortion removal. So the region will be misaligned.
        // So the solution is to apply acvVecRadialDistortionRemove for the init line_cand, that will compensate the problem.
        // Ideally, the line vector and region width should be compensated as well, but.... if the lens is really that bad, you better find a really good reason to use that.
        line_cand.line_anchor=acvVecRadialDistortionRemove(line_cand.line_anchor,param);

        float MatchingMarginX=line->MatchingMarginX*ppmm;
        float initMatchingMargin=line->initMatchingMargin*ppmm;

        s_points.resize(0);
        edge_grid.getContourPointsWithInLineContour(line_cand,
          MatchingMarginX,

          //HACK:Kinda hack... the initial Margin is for initial keypoints search, 
          //But since we get the cadidate line already, no need for huge Margin
          initMatchingMargin/2,
          flip_f,
          s_intersectIdxs,s_points);
          
        //LOGV("l-vec: %f %f l-anc:%f %f",line_cand.line_vec.X,line_cand.line_vec.Y,line_cand.line_anchor.X,line_cand.line_anchor.Y);
        LOGV("MatchingMarginX:%f s_points.size():%d initMatchingMargin:%f",
          MatchingMarginX,s_points.size(),initMatchingMargin);

       
        if(s_points.size()>5*4)
        {

          
          //LOGV("Line adj:thres:%f",thres);
          acv_XY lineNormal ={X:-line_cand.line_vec.Y,Y:line_cand.line_vec.X};
          int sptL=s_points.size();
          for(int k=0;k<s_points.size();k++)
          {
            acv_XY ret_point_opt;
            float edgeResponse;
            
            //remember!! the s_points[i].pt are the points after distortion removal
            //And the points on the image is still the distorted data
            acv_XY tmp_pt = acvVecRadialDistortionApply(s_points[k].pt,param);
            //int ret_val = EdgePointOpt(smoothedImg,lineNormal,tmp_pt,&ret_point_opt,&edgeResponse);
            int ret_val = EdgePointOpt2(smoothedImg,lineNormal,tmp_pt,3,thres,&ret_point_opt,&edgeResponse);
            if(ret_val==0)
            {
              int X_=round(ret_point_opt.X);
              int Y_=round(ret_point_opt.Y);

              s_points[k].pt = acvVecRadialDistortionRemove(ret_point_opt,param);
              s_points[k].edgeRsp = (edgeResponse<0)?-edgeResponse:edgeResponse;
              //LOGV("%f  %f",ret_point_opt.X,ret_point_opt.Y);
              if(drawDBG_IMG)
              {
                img->CVector[Y_][X_*3]=125;
                img->CVector[Y_][X_*3+1]=255;
                img->CVector[Y_][X_*3+2]=0;
                buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3]=0;
                buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3+1]=0;
                buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3+2]=255;
              }
            }
          }
          
          float minS_pts=0;
          float minSigma=99999;
          for(int m=0;m<7;m++)
          {
            int sampleL=s_points.size()/5;
            for(int k=0;k<sampleL;k++)//Shuffle in 
            {
              int idx2Swap = (rand()%(s_points.size()-k))+k;

              ContourGrid::ptInfo tmp_pt=s_points[k];
              s_points[k]=s_points[idx2Swap];
              s_points[idx2Swap]=tmp_pt;
              //s_points[j].edgeRsp=1;
            }

            acv_Line tmp_line;
            acvFitLine(
              &(s_points[0].pt)     ,sizeof(ContourGrid::ptInfo), 
              &(s_points[0].edgeRsp),sizeof(ContourGrid::ptInfo), sampleL,&tmp_line,&sigma);

            int sigma_count=0;
            float sigma_sum=0;
            for(int i=0;i<s_points.size();i++)
            {
              float diff =  acvDistance_Signed(tmp_line,s_points[i].pt);
              float abs_diff=(diff<0)?-diff:diff;
              if(abs_diff>2)
              {
                //s_points[j].edgeRsp=0;
                continue;
              }
              sigma_count++;
              sigma_sum+=diff*diff;
              
              //s_points[j].edgeRsp=1/(abs_diff*abs_diff+1);
            }
            sigma_sum=sqrt(sigma_sum/sigma_count);
            if(minSigma>sigma_sum)
            {
              minS_pts = sigma_count;
              minSigma = sigma_sum;
              line_cand = tmp_line;
              LOGV("minSigma:%f",minSigma);
            }
          }

          
          int usable_L=0;
          for(int k=0;k<2;k++)
          {
            usable_L=0;
            if(s_points.size()==0)
            {
              usable_L=0;
              FeatureReport_lineReport lr;
              lr.line=lf_zero;
              lr.def=line;
              lr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;//Not able to find starting point
              detectedLines.push_back(lr);
              LOGE("Not able to find starting point");
              break;
            }

            for(int n=0;n<s_points.size();n++)
            {
              float dist  = acvDistance_Signed(line_cand,s_points[n].pt);
              if(dist<0)dist=-dist;
              s_points[n].tmp =  dist;
              //s_points[i].edgeRsp=1;
              
            }
            
            std::sort(s_points.begin(), s_points.end(),  
                  [](const ContourGrid::ptInfo & a, const ContourGrid::ptInfo & b) -> bool
              { 
                  return a.tmp < b.tmp; 
              });
            
            float distThres = s_points[s_points.size()/2].tmp+1;
            LOGV("sort finish size:%d, distThres:%f",s_points.size(),distThres);

            for(int n=s_points.size()/2;n<s_points.size();n++)
            {
              usable_L=n;
              if(s_points[n].tmp>distThres)break;
            }
            //usable_L=usable_L*10/11;//back off
            LOGV("usable_L:%d/%d  minSigma:%f=>%f",
              usable_L,s_points.size(),
              s_points[usable_L-1].tmp,distThres);
            acvFitLine(
              &(s_points[0].pt)     ,sizeof(ContourGrid::ptInfo), 
              &(s_points[0].edgeRsp),sizeof(ContourGrid::ptInfo), 
              usable_L,&line_cand,&sigma);
          }
          
          if(usable_L==0)continue;

          if(0)
          {
            for(int n=0;n<usable_L;n++)
            {
              
              acv_XY tmp_pt = acvVecRadialDistortionApply(s_points[n].pt,param);
              int X_=round(tmp_pt.X);
              int Y_=round(tmp_pt.Y);
              smoothedImg->CVector[Y_][X_*3]=255;
              smoothedImg->CVector[Y_][X_*3+1]=0;
              smoothedImg->CVector[Y_][X_*3+2]=125;
            }
        
          }
        }
        else
        {
          acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, s_points.size(),&line_cand,&sigma);
        }




        /*acvDrawLine(buff_,
          line_cand.line_anchor.X-mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y-mult*line_cand.line_vec.Y,
          line_cand.line_anchor.X+mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y+mult*line_cand.line_vec.Y,
          20,255,128);*/


        if( acv2DDotProduct(line_cand.line_vec,target_vec)<0  )
        {
          LOGV("VEC INV::::");
          line_cand.line_vec = acvVecMult(line_cand.line_vec,-1);
        }
        if(0)//Draw debug image(curve and straight line)
        {
          for(int k=0;k<s_points.size();k++)
          {
            const ContourGrid::ptInfo pti= s_points[k];
            const acv_XY p = pti.pt;
            int X = round(p.X);
            int Y = round(p.Y);
            if(drawDBG_IMG)
            {
              buff_->CVector[Y][X*3]=255;
              buff_->CVector[Y][X*3+1]=100;
              buff_->CVector[Y][X*3+2]=255;
            }
          }
        }

        LOGV("L=%d===anchor:%f,%f vec:%f,%f ,sigma:%f target_vec:%f,%f",j,
        line_cand.line_anchor.X,
        line_cand.line_anchor.Y,
        line_cand.line_vec.X,
        line_cand.line_vec.Y,
        sigma,
        target_vec.X,target_vec.Y);


        //acv_XY *end_pos=findEndPoint(line_cand, 1, s_points);
        //acv_XY *end_neg=findEndPoint(line_cand, -1, s_points);

        acv_LineFit lf;
        lf.line=line_cand;
        lf.matching_pts=s_points.size();
        lf.s=sigma;
        //if(end_pos)lf.end_pos=*end_pos;
        //if(end_neg)lf.end_neg=*end_neg;


        LOGV("end_pos.X:%f end_pos.Y:%f end_neg.X:%f end_neg.Y:%f",
          lf.end_pos.X,
          lf.end_pos.Y,
          lf.end_neg.X,
          lf.end_neg.Y);

        FeatureReport_lineReport lr;
        lr.line=lf;
        lr.def=line;
        if(lf.matching_pts<5)
        {
          lr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
        }
        else
        {
          lr.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        }
        lr.line.end_neg=acvVecMult(lr.line.end_neg,0);
        lr.line.end_pos=acvVecMult(lr.line.end_pos,0);
        detectedLines.push_back(lr);
      }
      // char strdd[100];
      // sprintf(strdd,"data//ttt/MVCamX%d.bmp",rand()%200);
      // acvSaveBitmapFile(strdd,smoothedImg);
      //exit(1);

      acv_CircleFit cf_zero= {0};
      for (int j = 0; j < featureCircleList.size(); j++)
      {
        
        featureDef_circle &cdef= featureCircleList[j];
        
        float initMatchingMargin=cdef.initMatchingMargin*ppmm;
        

        acv_XY center = acvRotation(cached_sin,cached_cos,flip_f,cdef.circleTar.circumcenter);

        center = acvVecMult(center,ppmm);


        int matching_tor=initMatchingMargin;
        center=acvVecAdd(center,ldData[i].Center);
        
        LOGV("flip_f:%f angle:%f sAngle:%f  eAngle:%f",flip_f,angle,cdef.sAngle,cdef.eAngle);
        float sAngle,eAngle;
        if(flip_f>0)
        {
          sAngle = cdef.sAngle+angle;
          eAngle = cdef.eAngle+angle;
        }
        else
        {
          sAngle = -(cdef.eAngle-angle);
          eAngle = -(cdef.sAngle-angle);
        }
        LOGV("flip_f:%f angle:%f sAngle:%f  eAngle:%f",flip_f,angle,sAngle,eAngle);
        float radius = cdef.circleTar.radius*ppmm;

        edge_grid.getContourPointsWithInCircleContour(
          center.X,
          center.Y,
          radius,
          sAngle,eAngle,cdef.outter_inner,
          matching_tor,
          s_intersectIdxs,s_points);
        
        acv_CircleFit cf;
        circleRefine(s_points,&cf);
        // float minTor = matching_tor/5;
        // if(minTor<1)minTor=1;
        // edge_grid.getContourPointsWithInCircleContour(
        //   cf.circle.circumcenter.X,
        //   cf.circle.circumcenter.Y,
        //   cf.circle.radius,
        //   sAngle,eAngle,cdef.outter_inner,
        //   minTor,
        //   s_intersectIdxs,s_points);

        if(1)
        {
          for(int k=0;k<s_points.size();k++)
          {
            acv_XY ret_point_opt;
            float edgeResponse;
            acv_XY lineNormal ={X:-s_points[k].contourDir.Y,Y:s_points[k].contourDir.X};
            acv_XY tmp_pt= acvVecRadialDistortionApply(s_points[k].pt,param);
            int ret_val = EdgePointOpt(smoothedImg,lineNormal,tmp_pt,&ret_point_opt,&edgeResponse);
            s_points[k].edgeRsp=1;
            if(ret_val==0)
            {

              s_points[k].pt = acvVecRadialDistortionRemove(ret_point_opt,param);
              s_points[k].edgeRsp = (edgeResponse<0)?-edgeResponse:edgeResponse;
              //LOGV("%f  %f",ret_point_opt.X,ret_point_opt.Y);
              //buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3]=0;
              //buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3+1]=0;
              //buff_->CVector[(int)round(ret_point_opt.Y)][(int)round(ret_point_opt.X)*3+2]=255;
            }
          }
        }

        circleRefine(s_points,&cf);

        FeatureReport_circleReport cr;
        cr.circle=cf;
        cr.def = &cdef;
        if(cf.circle.radius != cf.circle.radius)//check NaN 
        {
        
          LOGV("Circle search failed: resultR:%f defR:%f",
          cf.circle.radius,cdef.circleTar.radius);
          cr.status = FeatureReport_sig360_circle_line_single::STATUS_NA;
          detectedCircles.push_back(cr);
          continue;
        }

        
        if(drawDBG_IMG)
        {

          acvDrawCrossX(buff_,
            center.X,center.Y,
            3,3);

          acvDrawCrossX(buff_,
            cf.circle.circumcenter.X,cf.circle.circumcenter.Y,
            5,3);

          acvDrawCircle(buff_,
          cf.circle.circumcenter.X, cf.circle.circumcenter.Y,
          cf.circle.radius,
          20,255, 0, 0);

        }

        LOGV("C=%d===%f,%f   => %f,%f, dist:%f matching_pts:%d",
        j,cdef.circleTar.circumcenter.X*ppmm,cdef.circleTar.circumcenter.Y*ppmm,
        center.X,center.Y,
        hypot(cf.circle.circumcenter.X-center.X,cf.circle.circumcenter.Y-center.Y),
        cf.matching_pts);


        if( cf.circle.radius<radius-initMatchingMargin||
        cf.circle.radius>radius+initMatchingMargin )
        {
          cr.status = FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
        }
        else
        {
          cr.status = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
        }
        detectedCircles.push_back(cr);
      }


      for(int j=0;j<searchPointList.size();j++)
      {
        featureDef_searchPoint spoint =searchPointList[j];
        spoint.margin*=ppmm;
        spoint.width*=ppmm;
        if(spoint.subtype == featureDef_searchPoint::anglefollow)
        {
          spoint.data.anglefollow.position=acvVecMult(spoint.data.anglefollow.position,ppmm);
        }
        FeatureReport_searchPointReport report= searchPoint_process(smoothedImg,img,i,ldData[i],singleReport,cached_sin,cached_cos,flip_f,thres,spoint,buff_);
        LOGV("id:%d, %d",report.def->id,searchPointList[j].id);
        report.def = &(searchPointList[j]);
        detectedSearchPoints.push_back(report);
      }
      for(int j=0;j<auxPointList.size();j++)
      {
        featureDef_auxPoint apoint =auxPointList[j];
        if(apoint.subtype == featureDef_auxPoint::lineCross)
        {
          //No value to convert, id only
        }
        else if(apoint.subtype == featureDef_auxPoint::centre )
        {
          //No value to convert, id only
        }
        
        FeatureReport_auxPointReport report= auxPoint_process(singleReport,cached_sin,cached_cos,flip_f,apoint);
        report.def = &(auxPointList[j]);
        detectedAuxPoints.push_back(report);
      }



      if(1)
      { 
        //Convert report to mm based unit
        for(int j=0;j<detectedLines.size();j++)
        {
          detectedLines[j].line.end_neg = acvVecMult(detectedLines[j].line.end_neg,mmpp);
          detectedLines[j].line.end_pos = acvVecMult(detectedLines[j].line.end_pos,mmpp);
          detectedLines[j].line.line.line_anchor = acvVecMult(detectedLines[j].line.line.line_anchor,mmpp);
          //detectedLines[i].line.line.line_vec = acvVecMult(detectedLines[i].line.line.line_vec,mmpp);
          detectedLines[j].line.s = detectedLines[j].line.s*mmpp;
        }
        for(int j=0;j<detectedCircles.size();j++)
        {
          detectedCircles[j].circle.s*=mmpp;
          detectedCircles[j].circle.circle.circumcenter=
            acvVecMult(detectedCircles[j].circle.circle.circumcenter,mmpp);
          detectedCircles[j].circle.circle.radius*=mmpp;
        }
        for(int j=0;j<detectedSearchPoints.size();j++)
        {
          detectedSearchPoints[j].pt=
            acvVecMult(detectedSearchPoints[j].pt,mmpp);
        }
        for(int j=0;j<detectedAuxPoints.size();j++)
        {
          detectedAuxPoints[j].pt=
            acvVecMult(detectedAuxPoints[j].pt,mmpp);
        }

        for(int j=0;j<judgeList.size();j++)
        {
          FeatureReport_judgeDef judge= judgeList[j];
          
          FeatureReport_judgeReport report= measure_process(singleReport,cached_sin,cached_cos,flip_f,judge);
          report.def = &(judgeList[j]);
          judgeReports.push_back(report);
        }
      }
     


  }



  {//convert pixel unit to mm
    float mmpp=param.mmpb2b/param.ppb2b;

  }

  //LOGI(">>>>>>>>");
  return 0;
}
