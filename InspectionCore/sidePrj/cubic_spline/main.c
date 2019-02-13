/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
using namespace std;



void spline10(float *f,int fL,float *edgeX)
{
    int n,i,j,k;
    const int L = 10;
    float h[L],a,b,c,d,sum,s[L]={0},x[L],F[L],p,m[L][L]={0},temp;
    n = fL;



    for(i=0;i<n;i++)
    {
        x[i]=i;
        printf("%f\n",f[i]);
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

    float maxEdge_response = 0;
    float maxEdge_offset=NAN;
    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        b=s[i]/2;
        c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
        d=f[i];
        bool zeroCross = (s[i+1]*s[i])<0;
        float response =  abs(s[i+1]-s[i]);
        printf("%f %f => %f \n",s[i],s[i+1],response);
        if(zeroCross || (s[i]==0 && i!=0))
        {
            float offset = s[i]/(s[i]-s[i+1]);
            if(maxEdge_response<response)
            {
                maxEdge_response=response;
                maxEdge_offset = i+offset;
            }

            float xi =offset;
            float firDir = 3*a*xi*xi+2*b*xi+c;
            float secDir = 6*a*xi+2*b;
            printf("cross: offset:%f\n",i+offset);
            printf("%f <<%f,%f\n",sum,firDir,secDir);
        }
    }

    printf("MAX rsp>>> %f %f\n",maxEdge_response,maxEdge_offset);
    *edgeX = maxEdge_offset;

    return ;
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
                float xi = p-x[i];
                sum=a*pow(xi,3)+b*pow(xi,2)+c*(xi)+d;

                if(SessionI!=i)
                {
                    printf("-----%d---a:%f b:%f  c:%f  d:%f--\n",i,a,b,c,d);
                    SessionI=i;
                }
                float firDir = 3*a*xi*xi+2*b*xi+c;
                float secDir = 6*a*xi+2*b;
                printf("%f <<%f,%f\n",sum,firDir,secDir);
            }
    }
}
int main()
{
    float f[]={138.164139, 118.298965, 82.842812, 25.000084, 27.482094, 10.453522, 14.436648};
    int fL = sizeof(f)/sizeof(f[0]);
    float edgeX;
    spline10(f,fL,&edgeX);
    
    printf("edgeX: %f\n",edgeX);
    return 0;
}
int mainX()
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
            f[i]=0;//0 1 2 3 4
        }
        else//5 6 7 8 9
        {
            f[i]=100;//4 3 2 1 0
        }
    }

    a = f[0];
    for(int i=0;i<L-1;i++)
    {
        f[i]=a;
        a+=0.2*(f[i+1]-a);
        printf("%f...\n",f[i]);
    }

    a = f[L-1];
    for(int i=L-1;i>=1;i--)
    {
        f[i]=a;
        a+=0.8*(f[i-1]-a);
        printf("%f...\n",f[i]);
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
        printf("%f %f => %f \n",s[i+1],s[i],abs(s[i+1]-s[i]));
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
                float xi = p-x[i];
                sum=a*pow(xi,3)+b*pow(xi,2)+c*(xi)+d;

                if(SessionI!=i)
                {
                    printf("-----%d---a:%f b:%f  c:%f  d:%f--\n",i,a,b,c,d);
                    SessionI=i;
                }
                float firDir = 3*a*xi*xi+2*b*xi+c;
                float secDir = 6*a*xi+2*b;
                printf("%f <<%f,%f\n",sum,firDir,secDir);
            }
    }
    //getch();
    return 0;
}

