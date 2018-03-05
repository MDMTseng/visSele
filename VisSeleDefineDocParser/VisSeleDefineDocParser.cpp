#include "visSeleDefineDocParser.h"
#include "logctrl.h"



visSeleDefineDocParser::visSeleDefineDocParser(const char *json_str)
{
  root= NULL;
  reload(json_str);
}

int visSeleDefineDocParser::parse_jobj()
{


}


int visSeleDefineDocParser::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  featureCircleList.resize(0);
  featureLineList.resize(0);

  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }


}
