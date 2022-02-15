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

	int lineCharCount;
	char line[MAX_LINE_SIZE + 2];
  int blockCount;
  int blockInitial[30];
	GCodeParser();
	bool addChar(char c);
  virtual void parseLine(){};
  virtual void onError(int code)=0;
	void INIT();
};
