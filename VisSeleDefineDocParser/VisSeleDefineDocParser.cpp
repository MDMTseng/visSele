#include "visSeleDefineDocParser.h"




visSeleDefineDocParser::visSeleDefineDocParser(const char *json_str)
{
      root = cJSON_Parse(json_str);
}
