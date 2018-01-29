
#include "circleFitting.h"

#include <time.h>

int main()
//             this code tests algebraic circle fits
{
    reals BenchmarkExampleDataX[6] {1.,2.,5.,7.,9.,3.};
    reals BenchmarkExampleDataY[6] {7.,6.,8.,7.,5.,7.};

    Data data1(6,BenchmarkExampleDataX,BenchmarkExampleDataY);
    Circle circle;
    cout.precision(7);
/*
       Test One:  benchmark example from the journal paper

       W. Gander, G. H. Golub, and R. Strebel,
       "Least squares fitting of circles and ellipses"
       BIT, volume 34, (1994), pages 558-578

          Correct answers:

  Kasa   fit:  center (4.921166,3.835123)  radius 4.123176  sigma 0.4800269

  Pratt  fit:  center (4.615482,2.807354)  radius 4.911302  sigma 0.4610572

  Taubin fit:  center (4.613933,2.795209)  radius 4.879213  sigma 0.4572958

  Hyper  fit:  center (4.615482,2.807354)  radius 4.827575  sigma 0.4571757
*/
    circle = CircleFitByKasa (data1);
    cout << "\nTest One:\n  Kasa   fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByPratt (data1);
    cout << "\n  Pratt  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByTaubin (data1);
    cout << "\n  Taubin fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByHyper (data1);
    cout << "\n  Hyper  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

//            Test Two:  a randomly generated data set

    Data data2(10);    //   specify the number of data points

//          use the c++ random number generator to simulate data coordinates

    srand ( (unsigned)time(NULL) );  //  seed the random generator
    SimulateRandom (data2,1.0);       //  this function is in Utilities.cpp

    circle = CircleFitByKasa (data2);
    cout << "\nTest Two:\n  Kasa   fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByPratt (data2);
    cout << "\n  Pratt  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByTaubin (data2);
    cout << "\n  Taubin fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByHyper (data2);
    cout << "\n  Hyper  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;
/*
       Test Thee:  benchmark example from the journal paper

       W. Pratt, "Direct least-squares fitting of algebraic surfaces"
       Computer Graphics, volume 21, (1987), pages 145-152.

       It demonstrates that Kasa fit may grossly underestimate the circle size

          Correct answers:

  Kasa   fit:  center (0.0083059,-0.724455)  radius 1.042973  sigma 0.2227621

  Pratt  fit:  center (0.4908357,-22.15212)  radius 22.18006  sigma 0.05416545

  Taubin fit:  center (0.4909211,-22.15598)  radius 22.18378  sigma 0.05416513

  Hyper  fit:  center (0.4908357,-22.15212)  radius 22.17979  sigma 0.05416513
*/

    reals BenchmarkExample2DataX[4] {-1.,-0.3,0.3,1.};
    reals BenchmarkExample2DataY[4] {0.,-0.06,0.1,0.};

    Data data3(4,BenchmarkExample2DataX,BenchmarkExample2DataY);

    circle = CircleFitByKasa (data3);
    cout << "\nTest Three:\n  Kasa   fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByPratt (data3);
    cout << "\n  Pratt  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByTaubin (data3);
    cout << "\n  Taubin fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;

    circle = CircleFitByHyper (data3);
    cout << "\n  Hyper  fit:  center ("
         << circle.a <<","<< circle.b <<")  radius "
         << circle.r << "  sigma " << circle.s << endl;
}
