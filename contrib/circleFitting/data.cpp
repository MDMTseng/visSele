#include "data.h"
/************************************************************************
			BODY OF THE MEMBER ROUTINES
************************************************************************/

// Constructor with assignment of the field N
Data::Data(int N)
{
  X=NULL;
  Y=NULL;
  reset();
  resize(N);
}

// Constructor with assignment of each field
Data::Data(int N, reals dataX[], reals dataY[])
{
  X=NULL;
  Y=NULL;
  reset();
  resize(N);
	for (int i=0; i<n; i++)
	{
		X[i]=dataX[i];
		Y[i]=dataY[i];
	}
}

// Routine that computes the x- and y- sample means (the coordinates of the centeroid)

void Data::means(void)
{
	meanX=0.; meanY=0.;

	for (int i=0; i<n; i++)
	{
		meanX += X[i];
		meanY += Y[i];
	}
	meanX /= n;
	meanY /= n;
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

  X=NULL;
  Y=NULL;
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
}



// Routine that centers the data set (shifts the coordinates to the centeroid)

void Data::center(void)
{
	reals sX=0.,sY=0.;
	int i;

	for (i=0; i<n; i++)
	{
		sX += X[i];
		sY += Y[i];
	}
	sX /= n;
	sY /= n;

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
