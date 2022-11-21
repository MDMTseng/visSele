#pragma once
#include <stdint.h>
const int MAX_LINE_SIZE = 256; // Maximun GCode line size.



class GCodeParser
{
protected:

  int blkHeadIdx;
  bool isInSpace;
  bool headParenthesesFound;
  bool semicolonFound;
  
public:
  typedef enum { 

    LINE_EMPTY= 9001,
    LINE_INCOMPLETE= 9000,


    TASK_WARN_OK= 100, 
    TASK_OK= 0, 
    TASK_FAILED= -1, 
    TASK_UNSUPPORTED= -3000, 


    GCODE_PARSE_ERROR= -6000, 
    TASK_FATAL_FAILED= -9000, 

  } GCodeParser_Status;


	int lineCharCount;
	char line[MAX_LINE_SIZE + 2];
  int blockCount;
  char* blockInitial[30];
	GCodeParser();
  GCodeParser_Status runLine(const char *line);
	GCodeParser_Status addChar(char c);

	GCodeParser_Status statusReducer(GCodeParser_Status st,GCodeParser_Status new_st);

  virtual int FindGMEnd_idx(char **blkIdxes,int blkIdxesL);
  virtual GCodeParser_Status parseLine();

  virtual GCodeParser_Status parseCMD(char **blks, char blkCount)=0;
  virtual void onError(int code)=0;
	void INIT();
};



int FindFloat(const char *prefix,char **blkIdxes,int blkIdxesL,float &retNum);
int FindInt32(const char *prefix,char **blkIdxes,int blkIdxesL,int32_t &retNum);

int FindUint32(const char *prefix,char **blkIdxes,int blkIdxesL,uint32_t &retNum);
int FindStr(const char *prefix,char **blkIdxes,int blkIdxesL,char* retStr);

bool FindExist(const char *prefix,char **blkIdxes,int blkIdxesL);
bool CheckHead(const char *str1,const char *str2);