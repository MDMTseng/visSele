#include <string>
#include <stdio.h>
using namespace std;
class acvMatrix
{
	double **Matrix;
	int Col,Row;
	public:
	acvMatrix(int Row,int Col)
	{
		this->Col=Col;
		this->Row=Row;
		Matrix=new double*[Row];
                Matrix[0]=new double[Row*Col];
                for(int i=1;i<Row;i++)
                {
                    Matrix[i]=Matrix[i-1]+Col;
                }
	}
	~acvMatrix()
	{
                delete [](*Matrix);
		delete []Matrix;
	}
        inline double** GetArr()
	{

                return Matrix;
	}
        inline int GetRow()
	{

                return Row;
	}
        inline int GetCol()
	{

                return Col;
	}
	static inline bool Mul(acvMatrix &AnsMatrix,acvMatrix &LeftMatrix,acvMatrix &RightMatrix)
	{
     		int i,j,k;


     		double (**LeftMat)=(LeftMatrix.Matrix);

     		double (**RightMat)=(RightMatrix.Matrix);
     		double (**MatArray)=(AnsMatrix.Matrix);

                int LeftRow=LeftMatrix.Row,RightRow=RightMatrix.Row;
                int LeftCol=LeftMatrix.Col,RightCol=RightMatrix.Col;

     		double *MatRowV,*LeftRowV;

                if(LeftCol!=RightRow)return false;

     		for(i=0;i<LeftRow;i++)
                {
                        MatRowV=MatArray[i];
                        LeftRowV=LeftMat[i];
                        for(j=0;j<RightCol;j++)
                        {
                                MatRowV[j]=0;
                                for(k=0;k<RightRow;k++)
                                {
                                        MatRowV[j]+=LeftRowV[k]*RightMat[k][j];

                                }
                        }
                }
                return true;
	}
        inline void ToZero()
	{
                int i=Col*Row-1;
                for(;i>=0;i--)Matrix[0][i]=0;
	}
        inline void CopyFrom(acvMatrix &Src)
	{
                int i=Col*Row-1;
                double**  SrcMat=Src.GetArr();
                for(;i>=0;i--)Matrix[0][i]=SrcMat[0][i];
	}


        inline bool ToIdentity()
	{
                if(Row!=Col)return false;
                ToZero();
                for(int i=0;i<Row;i++)Matrix[i][i]=1;
                return true;
	}

        bool Transport(acvMatrix &TranposeMat)
	{
                int i,j;
                if(TranposeMat.GetRow()!=Col||TranposeMat.GetCol()!=Row)
                        return false;
                double **TranMat=TranposeMat.GetArr();
                for(i=0;i<Row;i++)for(j=0;j<Col;j++)
                {
                    TranMat[j][i]=Matrix[i][j];
                }
                return true;
	}
        /*void Transport(acvMatrix &TranposeMat)
	{
                int i,j;
                double **TranMat=new double*[Col];
                double SwapTmp;
                *TranMat=*Matrix;
                for(i=1;i<Col;i++)
                {
                    TranMat[i]=TranMat[i-1]+Row;
                }
                for(i=0;i<Row;i++)for(j=0;j<Col;j++)
                {
                    SwapTmp=Matrix[i][j];
                    Matrix[i][j]=TranMat[j][i];
                    TranMat[j][i]=SwapTmp;
                }
		delete []Matrix;
                Matrix=TranMat;
                i=Row;
                Row=Col;
                Col=i;



	}*/

        inline string ToString()
	{
                string Tmp;
                //TextPool d;
                int i,j;
                char d2s[30];

                for(i=0;i<Row;i++,Tmp=Tmp+"\r\n")for(j=0;j<Col;j++)
                {
                        sprintf(d2s,"%10.5f",Matrix[i][j]);
                        Tmp=Tmp+d2s+"   ";
                }
                return Tmp;

	}
        inline void PesudoInverse(acvMatrix &Inv)
        {
               /*if(Row==Col)
               {

                        Inverse(Inv);
                        return;
               }*/
               acvMatrix TansMat(Col,Row);
               Transport(TansMat);
               acvMatrix *ATA;
               acvMatrix *InvATA;
               if(Row>=Col)
               {
                    ATA=new acvMatrix(Col,Col);
                    InvATA=new acvMatrix(Col,Col);

                    Mul(*ATA,TansMat,*this);
                    ATA->Inverse(*InvATA);
                    delete ATA;

                    Mul(Inv,*InvATA,TansMat);
                    delete InvATA;
                    return ;

               }
               else
               {
                    ATA=new acvMatrix(Row,Row);
                    InvATA=new acvMatrix(Row,Row);

                    Mul(*ATA,*this,TansMat);
                    ATA->Inverse(*InvATA);
                    delete ATA;

                    Mul(Inv,TansMat,*InvATA);
                    delete InvATA;
                    return ;
               }

        }
        inline void Inverse(acvMatrix &Inv)
        {
               Inv.ToIdentity();
               acvMatrix Tmp(Row,Col);
               Tmp.CopyFrom(*this);
               Tmp.LTM(Inv);
               Tmp.UTM(Inv);


        }
        private:
        inline void SwapRow(int Row1,int Row2)
        {
                double *TmpRow;

                TmpRow=Matrix[Row1];
                Matrix[Row1]=Matrix[Row2];
                Matrix[Row2]=TmpRow;
        }
        void UTM(acvMatrix &Process)  	//upper triangular matrix
	{
		double tmp,zero;
                //Process.CopyFrom(*this);
                double** ProMat=Process.GetArr();
		//Process.ToIdentity();



		for(int k=0;k<Row;k++) for(int i=k;i<Row;i++)
		{tmp=Matrix[i][k];
			for(int j=0;j<Row;j++)
			{
				if(i==k)
				{
                                        ProMat[k][j]=ProMat[k][j]/tmp;
					Matrix[k][j]/=tmp;
				}
				else
				{
                                        Matrix[i][j]-=tmp*Matrix[k][j];
                                        ProMat[i][j]=ProMat[i][j] - tmp*ProMat[k][j];
		}}}

	}
        void LTM(acvMatrix &Process)  	//upper triangular matrix
	{
		double tmp,zero;
                //Process.CopyFrom(*this);
                double** ProMat=Process.GetArr();
		//Process.ToIdentity();



                for(int k=Row-1;k>=0;k--) for(int i=k;i>=0;i--)
                {tmp=Matrix[i][k];
                        for(int j=Row-1;j>=0;j--)
			{
				if(i==k)
				{
                                        ProMat[k][j]=ProMat[k][j]/tmp;
					Matrix[k][j]/=tmp;
				}
				else
				{
                                        Matrix[i][j]-=tmp*Matrix[k][j];
                                        ProMat[i][j]=ProMat[i][j] - tmp*ProMat[k][j];
		}}}

	}




	/*acvMatrix operator*(const acvMatrix&C)
	{
                if()
		acvMatrix tmp(1,1);
                Mul(tmp,*this,C);
		return tmp;
	} */
};





