/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
using namespace std;
int main()
{
    char choice='y';
    int n,i,j,k;
    const int L = 10;
    float h[10],a,b,c,d,sum,s[L]={0},x[L],F[L],f[L],p,m[L][L]={0},temp;


    n = L;
    for(int i=0;i<L;i++)
    {
        x[i]=i;

        if(i<L/2)//0 1 2 3 4
        {
            f[i]=i;//0 1 2 3 4
        }
        else//5 6 7 8 9
        {
            f[i]=L-i-1+0.1;//4 3 2 1 0
        }
    }




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

    //***********  forward elimination **************//

    for(i=1;i<n-2;i++)
    {
        temp=(m[i+1][i]/m[i][i]);
        for(j=1;j<=n-1;j++)
            m[i+1][j]-=temp*m[i][j];
    }

    //*********** back ward substitution *********//
    for(i=n-2;i>0;i--)
    {
        sum=0;
        for(j=i;j<=n-2;j++)
            sum+=m[i][j]*s[j];
        s[i]=(m[i][n-1]-sum)/m[i][i];
    }


    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        printf("%f %f \n",s[i+1],s[i]);
    }
           
    int SessionI=-1;
    for(j=0;j<100;j++)
    {
        float p = j*10.0/100.0;
        for(i=0;i<n-1;i++)
            if(x[i]<=p&&p<=x[i+1])
            {
                a=(s[i+1]-s[i])/(6*h[i]);
                b=s[i]/2;
                c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
                d=f[i];
                sum=a*pow((p-x[i]),3)+b*pow((p-x[i]),2)+c*(p-x[i])+d;

                if(SessionI!=i)
                {
                    printf("-----%d---a:%f b:%f  c:%f  d:%f--\n",i,a,b,c,d);
                    SessionI=i;
                }
                printf("%f \n",sum);
            }
    }
    //getch();
    return 0;
}

