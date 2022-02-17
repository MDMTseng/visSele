#pragma once

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
  int blockInitial[30];
	GCodeParser();
  GCodeParser_Status runLine(char *line);
	GCodeParser_Status addChar(char c);

	GCodeParser_Status statusReducer(GCodeParser_Status st,GCodeParser_Status new_st);


  virtual GCodeParser_Status parseLine()=0;
  virtual void onError(int code)=0;
	void INIT();
};
