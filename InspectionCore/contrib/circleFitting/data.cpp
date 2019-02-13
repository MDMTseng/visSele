#include "data.h"
/************************************************************************
			BODY OF THE MEMBER ROUTINES
************************************************************************/

// Constructor with assignment of the field N
Data::Data(int N)
{
  X=NULL;
  Y=NULL;
  W=NULL;
  reset();
  resize(N);
}

// Constructor with assignment of each field
Data::Data(int N, reals dataX[], reals dataY[], reals dataW[])
{
  X=NULL;
  Y=NULL;
  W=NULL;
  reset();
  resize(N);
	for (int i=0; i<n; i++)
	{
		X[i]=dataX[i];
		Y[i]=dataY[i];
		W[i]=dataW[i];
	}
}

// Routine that computes the x- and y- sample means (the coordinates of the centeroid)

void Data::means(void)
{
	meanX=0.; meanY=0.;

	reals sumW=0;
	for (int i=0; i<n; i++)
	{
		sumW+=W[i];
		meanX += X[i]*W[i];
		meanY += Y[i]*W[i];
	}
	meanX /= sumW;
	meanY /= sumW;
}

void Data::resize(int new_size)
{
  if(real_n < new_size)
  {
    resize_force(new_size);
  }
  else
  {
    n=new_size;
  }
}


int Data::real_size(void)
{
  return real_n;
}
int Data::size(void)
{
  return n;
}

void Data::reset()
{
  //printf("%s\n",__func__);
  if(X != NULL)
	 delete[] X;
  if(Y != NULL)
   delete[] Y;
  if(W != NULL)
   delete[] W;

  X=NULL;
  Y=NULL;
  W=NULL;
  n=0;
  real_n=0;
}

void Data::resize_force(int new_size)
{
  //printf("%s:%d\n",__func__,new_size);
  reset();
  n=new_size;
  real_n=new_size;
  X = new reals[n];
  Y = new reals[n];
  W = new reals[n];
}



// Routine that centers the data set (shifts the coordinates to the centeroid)

void Data::center(void)
{
	reals sX=0.,sY=0.;
	int i;

	reals sumW=0;

	for (i=0; i<n; i++)
	{
		sumW += W[i];
		sX += X[i];
		sY += Y[i];
	}
	sX /= sumW;
	sY /= sumW;

	for (i=0; i<n; i++)
	{
		X[i] -= sX;
		Y[i] -= sY;
	}
	meanX = 0.;
	meanY = 0.;
}

// Routine that scales the coordinates (makes them of order one)

void Data::scale(void)
{
	reals sXX=0.,sYY=0.,scaling;
	int i;

	for (i=0; i<n; i++)
	{
		sXX += X[i]*X[i];
		sYY += Y[i]*Y[i];
	}
	scaling = sqrt((sXX+sYY)/n/Two);

	for (i=0; i<n; i++)
	{
		X[i] /= scaling;
		Y[i] /= scaling;
	}
}

// Printing routine

void Data::print(void)
{
	cout << endl << "The data set has " << n << " points with coordinates :"<< endl;

	for (int i=0; i<n-1; i++) cout << setprecision(7) << "(" << X[i] << ","<< Y[i] << "), ";

	cout << "(" << X[n-1] << ","<< Y[n-1] << ")\n";
}

// Destructor
Data::~Data()
{
	delete[] X;
  delete[] Y;
}
