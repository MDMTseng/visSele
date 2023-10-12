#pragma once
#include <stdint.h>
#include <GCodeParser.hpp>

#include "MSteppersV2.hpp"




int FindFloat(const char *prefix,char **blkIdxes,int blkIdxesL,float &retNum);
int FindInt32(const char *prefix,char **blkIdxes,int blkIdxesL,int32_t &retNum);

int FindUint32(const char *prefix,char **blkIdxes,int blkIdxesL,uint32_t &retNum);
int FindStr(const char *prefix,char **blkIdxes,int blkIdxesL,char* retStr);

bool FindExist(const char *prefix,char **blkIdxes,int blkIdxesL);
bool CheckHead(const char *str1,const char *str2);




int axisGDX2IDX(char *GDXCode,int fallback);
const char* axisIDX2GDX(int IDXCode);

MSTP_segment_extra_info ReadSegment_extra_info(char **blkIdxes,int blkIdxesL);
int ReadGVecData(char **blkIdxes,int blkIdxesL,xVec_f &vec,MSTP_segment_extra_info *moveInfo);