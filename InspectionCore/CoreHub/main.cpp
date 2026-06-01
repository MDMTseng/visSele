#include <stdio.h>
#include <unistd.h>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"

#include "cJSON.h"

#include "Data_Layer_Protocol.hpp"
#include "Data_Layer_PHY.hpp"
#include "CameraLayerManager.hpp"

#include <logctrl.h>
LOG_MODULE("core.boot");

extern "C" {
    #include "quickjs.h"
}


float randomGen(float from=0,float to=1)
{
  float r01=(rand()%1000000)/1000000.0;
  return r01*(to-from)+from;
}

int_fast32_t testPolyFit()
{
  srand(time(NULL));
  // These inputs should result in the following approximate coefficients:
  //         0.5           2.5           1.0        3.0
  //    y = (0.5 * x^3) + (2.5 * x^2) + (1.0 * x) + 3.0
  const int dataL=100;
  struct DATA_XY
  {
    float x;
    float y;
  }Data[dataL];


  float coeff[]={1.0001,2.5,-3,4,5};
  const int order = sizeof(coeff)/sizeof(coeff[0])-1;
  for(int i=0;i<dataL;i++)
  {
    Data[i].x=(i-dataL/2)*0.1;
    Data[i].y=polycalc(Data[i].x, coeff,order+1);//+randomGen(-1,1);
  }

  float resCoeff[order+1]={0}; // resulting array of coefs

  // Perform the polyfit
  int result = polyfit(&(Data[0].x),
                       &(Data[0].y),
                       NULL,
                       dataL,
                       order,
                       resCoeff,
                       sizeof(Data[0]),
                       sizeof(Data[0])
                       );




  printf("Original coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",coeff[order-i]);
  printf("\nNew coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",resCoeff[order-i]);
  printf("\n===========\n");

  
  for(int i=0;i<dataL;i++)
  {
    float pred_yData=polycalc(Data[i].x, resCoeff,order+1);

    LOGI("[%d]: x:%.5f   y:%.5f   _y:%.5f  diff:%.5f",i,Data[i].x,Data[i].y,pred_yData,pred_yData-Data[i].y);
  }

  return 0;
}

CameraLayer::status CameraLayer_Callback_CAM1(CameraLayer &cl_obj, int type, void *context)
{
  
  LOGI("type:%d\n",type);
  return CameraLayer::status::ACK;
}


CameraLayer::status CameraLayer_Callback_CAM2(CameraLayer &cl_obj, int type, void *context)
{
  
  LOGI("type:%d\n",type);
  return CameraLayer::status::ACK;
}


void camLyerTest()
{
  CameraLayerManager clm;
  clm.discover();
  printf("cam:\n%s\n",clm.genJsonStringList().c_str());
  CameraLayer *camLayer=clm.connectCamera(0,"",CameraLayer_Callback_CAM1,NULL);
  CameraLayer *camLayer2=clm.connectCamera(1,"data/BMP_carousel_test1",CameraLayer_Callback_CAM2,NULL);
  // CameraLayer *camLayer=clm.connectCamera(1,0,"data/BMP_carousel_test",CameraLayer_Callback_XXX,NULL);

  if(camLayer!=NULL)
  {
    printf("connected:\n%s\n",camLayer->getCameraJsonInfo().c_str());
  }
  else
  {
    printf("connect failed\n");
    return ;
  }
  // camLayer->TriggerMode(1);
  // printf("WAIT for image trigger~\n");
  // for(int i=0;i<2;i++)
  // {
  //   camLayer->Trigger();
  //   sleep(1);
  // }


  camLayer->TriggerMode(0);
  camLayer->SetFrameRate(NAN);
  

  camLayer2->TriggerMode(0);
  camLayer2->SetFrameRate(NAN);
  printf("WAIT for image streaming~\n");
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }
  
  camLayer->SetROI(0,0,100,100,100,100);
  printf("WAIT for image streaming~ setFPS:err:%d\n",camLayer->SetFrameRate(INFINITY));
  
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }


  camLayer->SetROI(0,0,100,100,100,100);
  printf("WAIT for image streaming~ setFPS:err:%d\n",camLayer->SetFrameRate(5));
  
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }

  delete camLayer;
}

void handle_exception(JSContext *ctx, JSValue exception) {
    const char *error_message = JS_ToCString(ctx, exception);
    std::cerr << "JavaScript Error: " << error_message << std::endl;
    JS_FreeCString(ctx, error_message);
    JS_FreeValue(ctx, exception);
}

void define_run_code_function(JSContext *ctx) {
    const char *script = R"(
        let funcList = {};

        let _g = {};
        let latestReturn = {};
        
        function registerFunc(name, code) {
            const func = new Function("params", code); // Create function from code
            funcList[name] = func;
        }

        function runCode(func_name, json) {
            const inputData = JSON.parse(json); // Parse JSON input
            const func = funcList[func_name]; // Retrieve function from funcList
            if (func === undefined) {
                return { error: "func not found" };
            }
            const result = func(inputData); // Execute code with input data
            latestReturn=result;
            return result; // Return result as JavaScript object
        }
    )";

    JSValue result = JS_Eval(ctx, script, strlen(script), "<script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        handle_exception(ctx, exception);
    }
    JS_FreeValue(ctx, result);
}

JSValue call_js_function(JSContext *ctx, const char *func_name, int argc, JSValue *argv) {
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue js_function = JS_GetPropertyStr(ctx, global_obj, func_name);

    if (!JS_IsFunction(ctx, js_function)) {
        std::cerr << func_name << " is not defined or not a function" << std::endl;
        JS_FreeValue(ctx, js_function);
        JS_FreeValue(ctx, global_obj);
        return JS_UNDEFINED;
    }

    JSValue result = JS_Call(ctx, js_function, global_obj, argc, argv);

    JS_FreeValue(ctx, js_function);
    JS_FreeValue(ctx, global_obj);

    return result;
}

void registerJFunc(JSContext *ctx, std::string func_name, std::string func_code) {
    JSValue args_register[2];
    args_register[0] = JS_NewString(ctx, func_name.c_str());
    args_register[1] = JS_NewString(ctx, func_code.c_str());

    JSValue register_result = call_js_function(ctx, "registerFunc", 2, args_register);

    JS_FreeValue(ctx, args_register[0]);
    JS_FreeValue(ctx, args_register[1]);
    JS_FreeValue(ctx, register_result);
}

std::string test_register_and_run(JSContext *ctx, std::string func_name, std::string func_param) {
    JSValue args_run[2];
    args_run[0] = JS_NewString(ctx, func_name.c_str());
    args_run[1] = JS_NewString(ctx, func_param.c_str());

    JSValue run_result = call_js_function(ctx, "runCode", 2, args_run);

    JS_FreeValue(ctx, args_run[0]);
    JS_FreeValue(ctx, args_run[1]);

    std::string result_str;
    if (JS_IsException(run_result)) {
        JSValue exception = JS_GetException(ctx);
        handle_exception(ctx, exception);
    } else {
        JSValue json_string = JS_JSONStringify(ctx, run_result, JS_UNDEFINED, JS_UNDEFINED);
        const char *result_cstr = JS_ToCString(ctx, json_string);

        // std::cout << "runCode result: " << (result_str ? result_str : "null") << std::endl;
        result_str=result_cstr ? result_cstr : "";
        JS_FreeCString(ctx, result_cstr);
        JS_FreeValue(ctx, json_string);
    }

    JS_FreeValue(ctx, run_result);
    return result_str;
}

int testQuickJS() {
    // Initialize QuickJS runtime and context
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    if (!rt || !ctx) {
        std::cerr << "Failed to initialize QuickJS" << std::endl;
        return -1;
    }

    // Define `runCode` and `registerFunc`
    define_run_code_function(ctx);

    // Register a function
    std::string func_name = "addNumbers";
    std::string func_code = R"(
        return { sum: params.a + params.b };
    )";
    registerJFunc(ctx, func_name, func_code);




    // Prepare input JSON
    std::string input_json = R"({"a": 5, "b": 10})";

    // Test the registered function
    test_register_and_run(ctx, func_name, input_json);



    {

      std::string func_name = "inject_Lib1";
      std::string func_code = R"(
          _g.lib1={
            add:function(params){
              return {sum:params.a+params.b};
            },
            sub:function(params){
              return params.a-params.b;
            }
          };
          return {success:true};
      )";
      registerJFunc(ctx, func_name, func_code);


      

      // Prepare input JSON
      std::string input_json = R"({})";

      // Test the registered function
      test_register_and_run(ctx, func_name, input_json);

    }

    {

      std::string func_name = "call_Lib1";
      std::string func_code = R"(
        let ttt=0;
          for(let i=0;i<10;i++){
            ttt++;
          }
          return {result:_g.lib1.add(params),ttt:ttt};
      )";
      registerJFunc(ctx, func_name, func_code);



      // Test the registered function
      LOGI("call_Lib1 result:%s",test_register_and_run(ctx, func_name,  R"({"a":5,"b":10})").c_str());

      LOGI("call_Lib1 result:%s",test_register_and_run(ctx, func_name,  R"({"a":50,"b":10})").c_str());

    }




    // Clean up
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return 0;
}

int main(int argc, char **argv)
{
  // return testQuickJS();
  return cp_main(argc, argv);
}

