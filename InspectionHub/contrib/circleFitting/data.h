//
//						 data.h
//

/************************************************************************
			DECLARATION OF THE CLASS DATA
************************************************************************/
// Class for Data
// A data has 5 fields:
//       n (of type int), the number of data points
//       X and Y (arrays of type reals), arrays of x- and y-coordinates
//       meanX and meanY (of type reals), coordinates of the centroid (x and y sample means)

#ifndef CIRCLE_FITTING_DATA_H
#define CIRCLE_FITTING_DATA_H
#include "DEFINES.h"
class Data
{

	int real_n;
public:

	int n;
	reals *X;		//space is allocated in the constructors
	reals *Y;		//space is allocated in the constructors
	reals *W;		//space is allocated in the constructors
	reals meanX, meanY;

	// constructors
	Data(int N);
	Data(int N, reals X[], reals Y[], reals W[]);

	void reset();
	void resize(int new_size);
	void resize_force(int new_size);
	int real_size(void);
	int size(void);
	// routines
	void means(void);
	void center(void);
	void scale(void);
	void print(void);

	// destructors
	~Data();
};



#endif
