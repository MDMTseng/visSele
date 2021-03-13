
//---------------------------------------------------------------------------
#ifndef DynamicArrayHpp
#define DynamicArrayHpp

//#include <stdio.h>
#define NULL 0
#define UNITINTAL 20

//#define DyArrayDataType int
#define DAData_Var(Array,Num)  *Array->GetPointer(Num)
template <class DyArrayDataType>

class DyArray
{
        class DANode
        {
                public:
                DyArrayDataType*Data;
                DANode*Front,*Rear;
                int DataL;
        };

       protected:
        int      UIANum
                ,NowPosition   //Start From 1
                ,ApparentLength,     //Array on using    [1 2 3 4 5 * * *]==>5
                UnitIntAL;

        DANode *BottomNode,*UpperNode,*NowNode,*TmpNode1,*TmpNode2;


        public:
        __fastcall DyArray();
        __fastcall DyArray(int UnitIntArrayL);
        __fastcall ~DyArray();
        void __fastcall DataFree();
        protected:
        void __fastcall Init();
        void __fastcall AddNodeNum(int Num);


        public:
        /*int __fastcall Get(int Index);
        int __fastcall Write(int Index,int Var);  */
        DyArrayDataType* __fastcall GetPointer(int Index);
        int __fastcall Length(void);
        int __fastcall TrueLength(void);
        void __fastcall ReLength(int NewLength);
        void __fastcall ForceFitLength(void);
        int __fastcall GetUnitIntArrayLength(void);
        DyArrayDataType* __fastcall Push();
        void __fastcall QueuePop();
        DyArrayDataType* __fastcall Front();
        DyArrayDataType* __fastcall Rear();
        void __fastcall StackPop();
        void __fastcall ClearLinearContainer();
        void __fastcall SetNowNode2Bottom(void);



};


#endif
