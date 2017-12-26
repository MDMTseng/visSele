

#include <libwebsockets.h>
#include <vector>

typedef struct ws_conn_info{
  void* user;
  struct lws *wsi;
}ws_conn_info;


class ws_conn_entity_pool{

    std::vector <ws_conn_info> ws_conn_set;


    ws_conn_info *find_avaliable_conn_info_slot()
    {
      	for(int i=0;i<ws_conn_set.size();i++)
      	{
      		if(ws_conn_set[i].user == NULL)
      			return &(ws_conn_set[i]);
      	}
      	ws_conn_info empty={0};
      	ws_conn_set.push_back(empty);

      	return &(ws_conn_set[ws_conn_set.size()-1]);
    }
    public:
    ws_conn_info *find(void* user)
    {
      	for(int i=0;i<ws_conn_set.size();i++)
      	{
      		if(ws_conn_set[i].user == user)
      			return &(ws_conn_set[i]);
      	}
      	return NULL;
    }



    int remove(void* user)
    {
        for(int i=0;i<ws_conn_set.size();i++)
        {
        	if(ws_conn_set[i].user == user)
          {
            ws_conn_info empty={0};
            ws_conn_set[i]=empty;
            return 0;
          }
        }
        return -1;
    }


    ws_conn_info* add(ws_conn_info info)
    {
        if(info.user == NULL || info.wsi == NULL )
          return NULL;

        if(find(info.user)!=NULL)
        {
          return NULL;
        }
        ws_conn_info* tmp = find_avaliable_conn_info_slot();
        *tmp = info;

      	return tmp;
    }

    int size()
    {
        int len=0;
        for(int i=0;i<ws_conn_set.size();i++)
        {
          if(ws_conn_set[i].user)
          {
        printf("fff\n");
            len++;
          }
        }
        return len;
    }
};
