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
    vec2 coorf=gl_FragCoord.xy;
    float reference_=texture(x3, coorf).x;
    vec2 ref_sobel_=texture(x4, coorf).xy;
    vec2 offset_=texture(x1, coorf).xy;//load
    {
      vec2 surPix=getSurPixAve(coorf,surPixR);
      offset_ =surPix+(offset_-surPix)*regRate;
    }
    float input_offset;
    for(int i=0;i<searchSteps;i++)//advance
    {
      input_offset=texture(x2, offset_+coorf).x;



      vec2 gradient=(lrate*(reference_-input_offset))*ref_sobel_;
      offset_+=gradient;

      /**/
      /**/
      /**/
    }

    y1.xy=offset_;//save
    //y2.xy=offset_*0.1+0.5;
    y2.x=(reference_-input_offset)*6+0.5;
}
