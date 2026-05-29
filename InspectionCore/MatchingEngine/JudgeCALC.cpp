#include "JudgeCALC.h"

#include <vector>
#include <string>
#include <cmath>
#include <cstdio>

using std::string;
using std::vector;

static int string_find_count(const char *str, char ch)
{
  int count = 0;
  for (int i = 0; str[i]; i++)
  {
    if (str[i] == ch)
    {
      count++;
    }
  }
  return count;
}

int parse_CALC_Id(const char *post_exp)
{
  if (post_exp[0] != '[')
    return -1;
  int idx = 0;
  for (int i = 1; post_exp[i]; i++)
  {
    char cc = post_exp[i];
    if ((cc < '0') || (cc > '9'))
      break;
    idx = idx * 10 + cc - '0';
  }
  return idx;
}

static bool isParamsCache(const char *exp)
{
  if (*exp != '$')
    return false;
  for (int i = 1; exp[i]; i += 2)
  {
    if (exp[i] != ',')
      return false;
    if (exp[i + 1] != '$')
      return false;
  }

  return true;
}

static bool strMatchExact(const char *src, const char *pat)
{
  for (int i = 0;; i++)
  {
    if (src[i] != pat[i])
      return false;
    if (src[i] == '\0')
      break;
  }
  return true;
}

static int functionExec_(const char *exp, float *params, int paramL, float *ret_result)
{
  if (ret_result)
    *ret_result = 0;
  if (strMatchExact(exp, "$+$"))
  {
    if (paramL != 2)
      return -1;
    *ret_result = params[0] + params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$-$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] - params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$*$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] * params[1];
    return 0;
  }
  else if (strMatchExact(exp, "$/$"))
  {

    if (paramL != 2)
      return -1;
    *ret_result = params[0] / params[1];
    return 0;
  }
  else if (strMatchExact(exp, "max$"))
  {
    float max = params[0];
    for (int i = 1; i < paramL; i++)
    {
      if (max < params[i])
        max = params[i];
    }
    *ret_result = max;
    return 0;
  }
  else if (strMatchExact(exp, "min$"))
  {
    float min = params[0];
    for (int i = 1; i < paramL; i++)
    {
      if (min > params[i])
        min = params[i];
    }
    *ret_result = min;
    return 0;
  }

  return -2;
}

int judge_CALC(FeatureReport_sig360_circle_line_single &reports, FeatureReport_judgeDef &judge, float *ret_result)
{
  vector<float> calcStack;

  //funcParamHeadIdx indicates the function params starts from
  //exp: [5,64,11]
  int funcParamCount = 0;

  //"exp": "max(sin([3]*3),0)",
  //"post_exp": ["[3]","3","$*$","sin$","0","$,$","max$"]
  for (int i = 0; i < judge.data.CALC.post_exp.size(); i++)
  {
    const string post_exp = judge.data.CALC.post_exp[i];
    //LOGI("post_exp[%d]:%s",i,post_exp.c_str());

    int id_exp = parse_CALC_Id(post_exp.c_str());
    if (id_exp >= 0)
    { //it's an id refence(  [4] means the result of judge report with id=4)
      int found_idx = -1;
      for (int j = 0; j < reports.judgeReports->size(); j++)
      {
        int id = (*reports.judgeReports)[j].def->id;
        if (id == id_exp)
        {
          found_idx = j;
        }
        else
          continue;
      }

      float val;
      if (found_idx >= 0)
      {
        int found_status = (*reports.judgeReports)[found_idx].status;
        if (found_status == FeatureReport_sig360_circle_line_single::STATUS_NA ||
            found_status == FeatureReport_sig360_circle_line_single::STATUS_UNSET)
        {
          if (ret_result)
            *ret_result = NAN;
          return -2;
        }
        else
          val = (*reports.judgeReports)[found_idx].measured_val;
      }
      else
      { //If there is no measure is found
        //val=NAN;
        if (ret_result)
          *ret_result = NAN;
        return -2;
      }
      calcStack.push_back(val);
      //(*reports.judgeReports)[]
    }
    else
    { //if it's not an id refence
      int paramSymbolCount = string_find_count(post_exp.c_str(), '$');
      if (paramSymbolCount > 0)
      { //if it's a function (sin$, cos$, max$)

        if (isParamsCache(post_exp.c_str()))
        { //If it's  $,$,.... just save the funcParamCount

          //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
          funcParamCount = paramSymbolCount;
        }
        else
        {
          if (funcParamCount > 1)
          {
            paramSymbolCount = funcParamCount;
          }
          funcParamCount = 1;

          //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
          float res;
          int err_code =
              functionExec_(
                  post_exp.c_str(),
                  &(calcStack[calcStack.size() - paramSymbolCount]),
                  paramSymbolCount,
                  &res);

          //LOGI("%f, %d",res,err_code);
          if (err_code != 0)
          {
            if (ret_result)
              *ret_result = NAN;
            return -50;
          }
          for (int k = 0; k < paramSymbolCount; k++)
          {
            calcStack.pop_back();
          }
          calcStack.push_back(res);
        }
      }
      else
      { //then it might be a number, try

        float val;
        try
        {
          val = std::stof(post_exp);
        }
        catch (...)
        {
          //val = NAN;
          if (ret_result)
            *ret_result = NAN;
          return -3;
        }
        calcStack.push_back(val);
      }
    }

    // for(int k=0;k<calcStack.size();k++)
    // {

    //   printf("%0.5f,",calcStack[k]);
    // }
    // printf("\n");
  }

  if (calcStack.size() != 1)
  {
    if (ret_result)
      *ret_result = NAN;
    return -50;
  }
  if (ret_result)
    *ret_result = calcStack[0];
  return 0;
}
