#ifndef Ext_Util_API_HPP
#define Ext_Util_API_HPP

#include <SOCK_Msg_Flow.hpp>
#include <mutex>

class json_seg_parser
{
    protected:
    char pch;
    int jsonInStrState;
    int jsonCurlyB_C;
    int jsonSquareB_C;

    public:
    json_seg_parser(){
        reset();
    }
    void reset()
    {
        pch='\0';
        jsonCurlyB_C=0;
        jsonInStrState=0;
        jsonSquareB_C=0;
    }
    int newChar(char ch){
        int ret_val=0;
        if(ch=='"' && pch!='\\')
        {
            jsonInStrState=!jsonInStrState;
        }

        if( (ch=='{' && pch=='}')||(ch=='[' && pch==']') )//End reset
        {
            reset();
            ret_val=0;
        }

        if(!jsonInStrState)
        {
            if( (ch=='{' || ch=='[') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
            {
                //start point
                ret_val=1;
            }
            switch(ch)
            {
                case '{':
                    jsonCurlyB_C++;
                break;
                case '[':
                    jsonSquareB_C++;
                break;
                case '}':
                    jsonCurlyB_C--;
                break;
                case ']':
                    jsonSquareB_C--;
                break;
            }
            
            if( (ch=='}' || ch==']') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
            {
                //end point
                ret_val=-1;
            }
            if(jsonCurlyB_C<0 || jsonSquareB_C<0 )//Error reset
            {
                reset();
                ret_val=0;
            }
        }
        pch=ch;
        return ret_val;
    }
};


class Ext_Util_API:public SOCK_Msg_Flow
{
    protected:
    json_seg_parser jsp;
    char jsonBuff[1024];
    char errorLock;
    public:

    std::timed_mutex syncLock;

    Ext_Util_API(char *host,int port) throw(int);

    int recv_data_thread();


    int cmd_cameraCalib(char* img_path, int board_w, int board_h);

    char* SYNC_cmd_cameraCalib(char* img_path, int board_w, int board_h);

    ~Ext_Util_API();
};


#endif