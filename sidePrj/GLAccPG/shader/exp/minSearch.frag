#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;//shiftMap
uniform sampler2DRect x2;//input
uniform sampler2DRect x3;//reference
uniform sampler2DRect x4;//reference sobel
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;//shiftMap
layout(location=1) out vec4 y2;//shiftMap

uniform int searchSteps=5;
uniform float lrate=120;
uniform float regRate=0.5;
uniform int surPixR=10;

vec2 getSurPixAve(vec2 coorf,int surR)
{
    return(
      texture(x1, coorf+vec2(-surR,0)).xy+
      texture(x1, coorf+vec2( surR,0)).xy+
      texture(x1, coorf+vec2(0,-surR)).xy+
      texture(x1, coorf+vec2(0, surR)).xy+

      (texture(x1, coorf+vec2(-surR,-surR)).xy+
      texture(x1, coorf+vec2( surR,-surR)).xy+
      texture(x1, coorf+vec2(-surR, surR)).xy+
      texture(x1, coorf+vec2( surR, surR)).xy)/2)/6;
}

void main()
{
float beta = 0.4;
float alpha = 1.0;
    vec2 coorf=gl_FragCoord.xy;
    float reference_=texture(x3, coorf).x;
    vec2 ref_sobel_=texture(x4, coorf).xy;
    vec4 offset_ori=texture(x1, coorf);

    vec2 offset_=offset_ori.xy;//loads
    vec2 pre_moment = offset_ori.zw;

    offset_+=alpha*beta*pre_moment;//Nesterov
    float input_offset;
    for(int i=0;i<searchSteps;i++)//advance
    {
      input_offset=texture(x2, offset_+coorf).x;
      vec2 gradient=(lrate*(reference_-input_offset))*ref_sobel_;
      offset_+=gradient;

    }
    vec2 grad = (offset_-offset_ori.xy);



    if(true){
      float grad_l = length(grad);
      if(grad_l>0.01)
        grad=grad/grad_l*pow(grad_l,0.8);
    }

    vec2 dirtest = pre_moment*grad;
    if(dirtest.x<0)pre_moment.x=grad.x;
    if(dirtest.y<0)pre_moment.y=grad.y;


    y1.xy=offset_+alpha*pre_moment;//Update/Save param

    vec2 moment=beta*pre_moment + grad;//Momentum
    y1.zw =moment;


    y2.r=(reference_-input_offset)*30;
    if(y2.r<0)y2.r*=-1;

    //y2.gb = ref_sobel_.xy/1+0.5;
    //y2.gb = y1.xy/150+0.5;


}
