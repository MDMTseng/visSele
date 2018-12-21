#include "FeatureManager_sig360_circle_line.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <stdio.h>


static int searchP(acvImage *img, acv_XY *pos, acv_XY searchVec, float maxSearchDist);
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

bool FeatureManager_sig360_circle_line::check(cJSON *root)
{
  char *str;
  LOGI("FeatureManager_sig360_circle_line>>>");
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("sig360_circle_line",str) == 0)
  {
    return true;
  }
  return false;
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
  LOGV("val:%f  margin:%f",judge.targetVal,judge.targetVal_margin);

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
        angleDiff = judgeReport.measured_val - judgeReport.def->targetVal;
        if(angleDiff<-180)angleDiff+=360;
        if(angleDiff>180)angleDiff-=360;
        if(angleDiff<0)angleDiff=-angleDiff;
        if(angleDiff>90)angleDiff=180-angleDiff;
        if(angleDiff>judgeReport.def->targetVal_margin)
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

        ret = ParseMainVector(flip_f,report,judge.OBJ1_id, &vec1);
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
        float diff = judgeReport.measured_val - judgeReport.def->targetVal;
        if(diff < 0)diff = -diff;
        
        if(diff>judgeReport.def->targetVal_margin)
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
      
    
      float diff = judgeReport.measured_val - judgeReport.def->targetVal;
      if(diff < 0)diff = -diff;
      if(diff>judgeReport.def->targetVal_margin)
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
  (acvImage *labeledImg,int labelId,acv_LabeledData labeledData,FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
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

        acv_XY searchVec_nor_edge = acvVecMult(searchVec_nor,-width/2);
        searchStart=acvVecAdd(searchStart,searchVec_nor_edge);
        

        LOGV("searchStart:%f %f",searchStart.X,searchStart.Y);
        acv_XY searchPt = searchStart;//Find from end line and approach to the end line;
        acv_XY searchPt_sum={0};
        int foundC =0;
        
        LOGV("searchPt:%f %f",searchPt.X,searchPt.Y);
        for(int i=0;i<margin*2;i++)
        {
          for(int j=0;j<width;j++)
          {
            searchPt = acvVecAdd(searchPt,searchVec_nor);
            int Y = (int)round(searchPt.Y);
            int X = (int)round(searchPt.X);

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
                searchPt_sum.X += X;
                searchPt_sum.Y += Y;
                foundC++;
              }
            }
          }
          if(foundC!=0)break;
          searchStart = acvVecAdd(searchStart,searchVec);
          searchPt = searchStart;
        }
        if(foundC!=0)
        {
          rep.pt =acvVecMult(searchPt_sum,1.0/foundC);
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

  line.initMatchingMargin=(int)*JFetEx_NUMBER(line_obj,"margin");

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

  judge.targetVal_margin=*JxNUM(judge_obj,"margin");

  
  judge.OBJ1_id = (int)*JxNUM(judge_obj,"ref[0].id");

  pnum = JFetch_NUMBER(judge_obj,"ref[1].id");//It's fine if we don't have OBJ2(ref[1])
  if(pnum == NULL)judge.OBJ2_id = -1;
  else {judge.OBJ2_id = *pnum;}

  LOGV("value:%f margin:%f id1:%d id2:%d",judge.targetVal,judge.targetVal_margin,judge.OBJ1_id,judge.OBJ2_id);
  judgeList.push_back(judge);
  return 0;
}



int FeatureManager_sig360_circle_line::parse_jobj()
{
  const char *type_str= JFetch_STRING(root,"type");
  const char *ver_str = JFetch_STRING(root,"ver");
  const char *unit_str =JFetch_STRING(root,"unit");
  const double *mmpp  = JFetch_NUMBER(root,"mmpp");
  if(type_str==NULL||ver_str==NULL||unit_str==NULL||mmpp==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  unit:<%p> mmpp(number):<%p>",type_str,ver_str,unit_str,mmpp);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  unit:<%s> mmpp(number):%f",type_str,ver_str,unit_str,*mmpp);


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

int FeatureManager_sig360_circle_line::FeatureMatching(acvImage *img,acvImage *buff_,vector<acv_LabeledData> &ldData,acvImage *dbg)
{

  int grid_size = 50;

  buff_->ReSize(img->GetWidth(),img->GetHeight());
  buff1.ReSize(img->GetWidth(),img->GetHeight());
  buff2.ReSize(img->GetWidth(),img->GetHeight());
  acvImage *buff = &buff1;
  acvCloneImage( img,buff_, -1);
  acvCloneImage( img,buff, -1);

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


  for (int i = 1; i < ldData.size(); i++,count++)
  {
      LOGI("Lable:%2d area:%d",i,ldData[i].area);
      if(ldData[i].area<120)continue;




      acvContourCircleSignature(img, ldData[i], i, tmp_signature);

      bool isInv;
      float angle;
      float error = SignatureMinMatching( tmp_signature,feature_signature,
        &isInv, &angle);

      error = sqrt(error)/feature_signature_ave;
      LOGV("======%d===er:%f,inv:%d,angDeg:%f",i,error,isInv,angle*180/3.14159);

      if(error>0.5)continue;
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
          .area=ldData[i].area,
          .rotate=angle,
          .isFlipped=isInv,
          .scale=1,
          .targetName=NULL,
      };


      LOGV("======%d===er:%f,inv:%d,angDeg:%f",i,error,isInv,angle*180/3.14159);
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
      extractLabeledContourDataToContourGrid(originalImage,buff,i,ldData[i],
        grid_size,edge_grid,scanline_skip);
      
      
      
      if(1)//Draw debug image(curve and straight line)
      {
        for(int i=0;i<edge_grid.dataSize();i++)
        {
          const ContourGrid::ptInfo pti= *edge_grid.get(i);
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

          
          LOGV("line->lineTar.line_vec: %f %f",line_cand.line_vec.X,line_cand.line_vec.Y);
            
          target_vec = line_cand.line_vec;
          line_cand.line_anchor =acvRotation(cached_sin,cached_cos,flip_f,line->searchEstAnchor);
          
          //Offet to real image and backoff searchDist distance along with the searchVec as start
          line_cand.line_anchor.X+=ldData[i].Center.X;
          line_cand.line_anchor.Y+=ldData[i].Center.Y;
          
          
          
          acv_XY searchVec;
          searchVec = acvRotation(cached_sin,cached_cos,flip_f,line->searchVec);

          s_points.resize(0);
          for(int k=0;k<line->keyPtList.size();k++)
          {
            featureDef_line::searchKeyPoint skp= line->keyPtList[k];

            skp.keyPt= acvRotation(cached_sin,cached_cos,flip_f,skp.keyPt);
            skp.keyPt.X+=ldData[i].Center.X;
            skp.keyPt.Y+=ldData[i].Center.Y;
            acvDrawCrossX(buff_,
              skp.keyPt.X,skp.keyPt.Y,
              2,2);
            if(searchP(img, &skp.keyPt , searchVec, line->searchDist)!=0)
            {
              continue;
            }
            acvDrawCrossX(buff_,
              skp.keyPt.X,skp.keyPt.Y,
              2,4);
            
                  
            ContourGrid::ptInfo pti= {pt:skp.keyPt};

            s_points.push_back(pti);
          }
          if(s_points.size()>2)
          {
            //Use founded points to fit a candidate line
            acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, 
              s_points.size(),&line_cand,&sigma);
            
            LOGV(" %f %f, %f %f",line_cand.line_vec.X,line_cand.line_vec.Y,target_vec.X,target_vec.Y);
            if( acv2DDotProduct(line_cand.line_vec,target_vec)<0 )
            {
              line_cand.line_vec = acvVecMult(line_cand.line_vec,-1);
            }
          }
          else if(s_points.size()==1)
          {
            //Use the 
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



        s_points.resize(0);
        edge_grid.getContourPointsWithInLineContour(line_cand,
          line->MatchingMarginX,
          line->initMatchingMargin,
          flip_f,
          s_intersectIdxs,s_points);
        acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, s_points.size(),&line_cand,&sigma);
        //LOGV("Matched points:%d",s_points.size());

        acvDrawLine(buff_,
          line_cand.line_anchor.X-mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y-mult*line_cand.line_vec.Y,
          line_cand.line_anchor.X+mult*line_cand.line_vec.X,
          line_cand.line_anchor.Y+mult*line_cand.line_vec.Y,
          20,255,128);


        if( acv2DDotProduct(line_cand.line_vec,target_vec)<0  )
        {
          LOGV("VEC INV::::");
          line_cand.line_vec = acvVecMult(line_cand.line_vec,-1);
        }
        if(1)//Draw debug image(curve and straight line)
        {
          for(int i=0;i<s_points.size();i++)
          {
            const ContourGrid::ptInfo pti= s_points[i];
            const acv_XY p = pti.pt;
            int X = round(p.X);
            int Y = round(p.Y);

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
        detectedLines.push_back(lr);
      }

      acv_CircleFit cf_zero= {0};
      for (int j = 0; j < featureCircleList.size(); j++)
      {
        featureDef_circle &cdef= featureCircleList[j];
        acv_XY center = acvRotation(cached_sin,cached_cos,flip_f,cdef.circleTar.circumcenter);

        int matching_tor=cdef.initMatchingMargin;
        center.X+=ldData[i].Center.X;
        center.Y+=ldData[i].Center.Y;
        
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
        

        edge_grid.getContourPointsWithInCircleContour(
          center.X,
          center.Y,
          cdef.circleTar.radius,
          sAngle,eAngle,cdef.outter_inner,
          matching_tor*2,
          s_intersectIdxs,s_points);

        acv_CircleFit cf;
        circleRefine(s_points,&cf);

        edge_grid.getContourPointsWithInCircleContour(
          cf.circle.circumcenter.X,
          cf.circle.circumcenter.Y,
          cf.circle.radius,
          sAngle,eAngle,cdef.outter_inner,
          matching_tor,
          s_intersectIdxs,s_points);
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


        LOGV("C=%d===%f,%f   => %f,%f, dist:%f matching_pts:%d",
        j,cdef.circleTar.circumcenter.X,cdef.circleTar.circumcenter.Y,center.X,center.Y,
        hypot(cf.circle.circumcenter.X-center.X,cf.circle.circumcenter.Y-center.Y),
        cf.matching_pts);


        if( cf.circle.radius<cdef.circleTar.radius-cdef.initMatchingMargin||
        cf.circle.radius>cdef.circleTar.radius+cdef.initMatchingMargin )
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
        FeatureReport_searchPointReport report= searchPoint_process(img,i,ldData[i],singleReport,cached_sin,cached_cos,flip_f,searchPointList[j],buff_);
        LOGV("id:%d, %d",report.def->id,searchPointList[j].id);
        detectedSearchPoints.push_back(report);
      }
      for(int j=0;j<auxPointList.size();j++)
      {
        FeatureReport_auxPointReport report= auxPoint_process(singleReport,cached_sin,cached_cos,flip_f,auxPointList[j]);
        detectedAuxPoints.push_back(report);
      }
      for(int j=0;j<judgeList.size();j++)
      {
        FeatureReport_judgeReport report= measure_process(singleReport,cached_sin,cached_cos,flip_f,judgeList[j]);
        judgeReports.push_back(report);
      }

  }


  //LOGI(">>>>>>>>");
  return 0;
}
