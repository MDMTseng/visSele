#include "DynamicArray.cpp"
#pragma argsused

#include <vcl.h>

int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
  return 1;
}
       //public:    
        __declspec(dllexport)
        template <class DyArrayDataType>__fastcall DyArray<DyArrayDataType>::DyArray()
        {
             UnitIntAL=UNITINTAL;
             TmpNode1=TmpNode2;
             Init();

        }
        __declspec(dllexport)template <class DyArrayDataType>__fastcall DyArray<DyArrayDataType>::DyArray(int UnitIntArrayL)
        {
             if(UnitIntArrayL<1)        UnitIntAL=UNITINTAL;
             else                    UnitIntAL=UnitIntArrayL;
             Init();

        }
        //protected: 
        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::DataFree()
        {
             ReLength(0);
             ForceFitLength();
             //if()
            // delete []GetPointer(0);

        }
        
        __declspec(dllexport)
        template <class DyArrayDataType>
        __fastcall DyArray<DyArrayDataType>::~DyArray()
        {
             DataFree();
        }



                   
        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::Init()
        {
             ApparentLength=0;
             //BottomPointer=new int[UnitIntAL+1];
             NowNode=UpperNode=BottomNode=NULL;
             UIANum=
             NowPosition=0;
             //AddNodeNum(1);

        }

        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::AddNodeNum(int Num)
        {
             if(BottomNode==NULL)
             {
                NowNode=UpperNode=BottomNode=new DANode();
                NowNode->Data=new DyArrayDataType[UnitIntAL];
                NowNode->DataL=UnitIntAL;

                UpperNode->Rear=NULL;
                BottomNode->Front=NULL;
                ApparentLength=UnitIntAL;
                NowPosition=0;
                UIANum=1;
                AddNodeNum(Num-1);
             }
             else
             {
                for(int i=0;i<Num;i++)
                {
                        TmpNode1=new DANode();//Creating new node
                        TmpNode1->Data=new DyArrayDataType[UnitIntAL];
                                                        // (new node) load memory
                        UpperNode->Rear=TmpNode1;  //UpperNode rear link to new node
                        TmpNode1->Front=UpperNode; //new node front link to UpperNode
                        UpperNode=TmpNode1;        //new node become to UpperNode
                        UpperNode->DataL=UnitIntAL;
                }
                NowNode=UpperNode;
                UpperNode->Rear=NULL;
                UIANum+=Num;
                NowPosition=UIANum-1;
             }
        }


        //public:
        /*__declspec(dllexport) int __fastcall DyArray<DyArrayDataType>::Get(int Index)
        {
             if(Index<0||Index>=UIANum*UnitIntAL)return 0;
             return   *GetPointer(Index);
        }


        __declspec(dllexport) int __fastcall DyArray<DyArrayDataType>::Write(int Index,int Var)
        {
             if(Index<0)return -1;

             if(Index>=UIANum*UnitIntAL)
                        AddLength((Index-UIANum*UnitIntAL)/UnitIntAL+1);

             *GetPointer(Index)=Var;


             return   0;
        } */




        __declspec(dllexport)
        template <class DyArrayDataType>
        DyArrayDataType* __fastcall DyArray<DyArrayDataType>::GetPointer(int Index)
        {
             if(Index<0)return NULL;
             int IndexPosition=Index/UnitIntAL;
             if(ApparentLength<Index+1)ApparentLength=Index+1;

             if(IndexPosition>=UIANum)
             {
                AddNodeNum(IndexPosition-UIANum+1);
                return (DyArrayDataType*)(UpperNode->Data)+(Index-IndexPosition*UnitIntAL);
             }
             if(IndexPosition>NowPosition)
             {//Node Searching forword

                //if(UIANum-1-IndexPosition<IndexPosition-NowPosition)//Closing to Top
                if(UIANum+NowPosition<IndexPosition*2)
                {
                        NowNode=UpperNode;
                        for(int i=UIANum-1;i>IndexPosition;i--)
                                NowNode=NowNode->Front;
                }
                else
                {
                        for(int i=NowPosition;i<IndexPosition;i++)
                                NowNode=NowNode->Rear;

                }
             }
             else
             {
                if(NowPosition>IndexPosition*2)//Closing to Bottom
                {
                        NowNode=BottomNode;
                        for(int i=0;i<IndexPosition;i++)
                                NowNode=NowNode->Rear;
                }
                else
                {
                        for(int i=NowPosition;i>IndexPosition;i--)
                                NowNode=NowNode->Front;

                }
             }
             NowPosition=IndexPosition;
             TmpNode1= NowNode;
             return  TmpNode1->Data+Index-IndexPosition*UnitIntAL;
        }

            
        __declspec(dllexport)
        template <class DyArrayDataType>
        int __fastcall DyArray<DyArrayDataType>::Length(void)
        {return ApparentLength;}
          
        __declspec(dllexport)
        template <class DyArrayDataType>
        int __fastcall DyArray<DyArrayDataType>::TrueLength(void)
        {return  UIANum*UnitIntAL;}
          
        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::ReLength(int NewLength)
        {
               if(NewLength<0)NewLength=0;
               ApparentLength=NewLength;


        }
        /*
        
        int      UIANum
                ,NowPosition
                ,ApparentLength;

        DANode *BottomNode;

        DANode *UpperNode;
        int *NowNode;
        int *TmpNode1,*TmpNode2;
        */


              
        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::ForceFitLength(void)
        {
                int DelUIANum=UIANum-(ApparentLength-1)/UnitIntAL-1;
                UIANum-=DelUIANum;

                for(;DelUIANum>0;DelUIANum--)
                {
                        NowNode=UpperNode;
                        if(!NowNode)
                        {
                                DelUIANum=0;
                                break;
                        }
                        delete []NowNode->Data; //Deleting the  UpperNode Data
                        UpperNode=UpperNode->Front;//UpperNode become previous node
                        delete(NowNode);       //Delete
                }
                NowPosition=0;
                NowNode=BottomNode;
                if(UpperNode)
                {
                        UpperNode->Rear=NULL;
                        NowNode=BottomNode;
                }
                else
                {
                        NowNode=BottomNode=NULL;
                }
        }


         
        __declspec(dllexport)
        template <class DyArrayDataType>
        int __fastcall DyArray<DyArrayDataType>::GetUnitIntArrayLength()
        {
                return UnitIntAL;
        }

        __declspec(dllexport)
        template <class DyArrayDataType>
        DyArrayDataType* __fastcall DyArray<DyArrayDataType>::Push()//Adding Data to Rear
        {
                ApparentLength+=UnitIntAL;
                if(ApparentLength==UnitIntAL)
                {
                        if(!NowNode)
                                AddNodeNum(1);
                        NowNode=BottomNode;
                        NowPosition=0;
                        return NowNode->Data;
                }
                if(!NowNode||!NowNode->Rear)
                        AddNodeNum(1);
                else
                {
                        NowNode=NowNode->Rear;
                        NowPosition++;
                }

                return NowNode->Data;
        }

        __declspec(dllexport)
        template <class DyArrayDataType>
        DyArrayDataType* __fastcall DyArray<DyArrayDataType>::Front()//Read the Front Data  (Queue)
        {

                return BottomNode->Data;
        }
        __declspec(dllexport)
        template <class DyArrayDataType>
        DyArrayDataType* __fastcall DyArray<DyArrayDataType>::Rear()//Read the Rear Data    (Stack)
        {

                return NowNode->Data;
        }

	__declspec(dllexport) 
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::QueuePop()//Deleting the Data of Front
        {
                if(ApparentLength)
                {
                        ApparentLength-=UnitIntAL;
                        NowPosition--;

                        if(ApparentLength)
                        {
                                TmpNode1=BottomNode;


                                BottomNode=BottomNode->Rear;
                                BottomNode->Front=NULL;

                                UpperNode->Rear=TmpNode1;
                                TmpNode1->Front=UpperNode;
                                UpperNode=TmpNode1;
                                UpperNode->Rear=NULL;
                        }
                }
        }
	__declspec(dllexport) 
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::StackPop()//Deleting if Data of Rear
        {
                if(ApparentLength)
                {
                        ApparentLength-=UnitIntAL;
                        NowPosition--;
                        if(ApparentLength)
                                NowNode=NowNode->Front;


                }
        }
        __declspec(dllexport) 
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::SetNowNode2Bottom(void)
        {

                NowPosition=0;
                NowNode=BottomNode;
        }

        __declspec(dllexport)
        template <class DyArrayDataType>
        void __fastcall DyArray<DyArrayDataType>::ClearLinearContainer(void)
        {

                ApparentLength=0;
        }

