#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef GLFW_DLL
#define GLFW_DLL
#endif

#include <windows.h>
#include <wincodec.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

// ==========================================
// 1. OpenGL 2.1 着色器函数
// Windows 自带的 gl.h 不声明这些函数，
// 因此通过 GLFW 动态获取。
// ==========================================

using CreateShaderFn = GLuint(APIENTRY*)(GLenum);
using ShaderSourceFn = void(APIENTRY*)(
    GLuint, GLsizei, const char* const*, const GLint*);
using CompileShaderFn = void(APIENTRY*)(GLuint);
using GetShaderivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetShaderLogFn = void(APIENTRY*)(
    GLuint, GLsizei, GLsizei*, char*);
using DeleteShaderFn = void(APIENTRY*)(GLuint);

using CreateProgramFn = GLuint(APIENTRY*)();
using AttachShaderFn = void(APIENTRY*)(GLuint, GLuint);
using LinkProgramFn = void(APIENTRY*)(GLuint);
using GetProgramivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetProgramLogFn = void(APIENTRY*)(
    GLuint, GLsizei, GLsizei*, char*);
using UseProgramFn = void(APIENTRY*)(GLuint);
using DeleteProgramFn = void(APIENTRY*)(GLuint);

using GetUniformLocationFn = GLint(APIENTRY*)(
    GLuint, const char*);
using Uniform1iFn = void(APIENTRY*)(GLint, GLint);
using Uniform3fFn = void(APIENTRY*)(
    GLint, GLfloat, GLfloat, GLfloat);

CreateShaderFn pCreateShader = nullptr;
ShaderSourceFn pShaderSource = nullptr;
CompileShaderFn pCompileShader = nullptr;
GetShaderivFn pGetShaderiv = nullptr;
GetShaderLogFn pGetShaderLog = nullptr;
DeleteShaderFn pDeleteShader = nullptr;

CreateProgramFn pCreateProgram = nullptr;
AttachShaderFn pAttachShader = nullptr;
LinkProgramFn pLinkProgram = nullptr;
GetProgramivFn pGetProgramiv = nullptr;
GetProgramLogFn pGetProgramLog = nullptr;
UseProgramFn pUseProgram = nullptr;
DeleteProgramFn pDeleteProgram = nullptr;

GetUniformLocationFn pGetUniformLocation = nullptr;
Uniform1iFn pUniform1i = nullptr;
Uniform3fFn pUniform3f = nullptr;

const GLenum VERTEX_SHADER = 0x8B31;
const GLenum FRAGMENT_SHADER = 0x8B30;
const GLenum COMPILE_STATUS = 0x8B81;
const GLenum LINK_STATUS = 0x8B82;

template<typename T>
bool loadFunction(T& function, const char* name)
{
    function = reinterpret_cast<T>(glfwGetProcAddress(name));
    if (!function)
        std::cerr << "Missing OpenGL function: " << name << "\n";
    return function != nullptr;
}

bool initShaderFunctions()
{
    bool ok = true;

#define LOAD(fn, name) \
    if (!loadFunction(fn, name)) ok = false

    LOAD(pCreateShader, "glCreateShader");
    LOAD(pShaderSource, "glShaderSource");
    LOAD(pCompileShader, "glCompileShader");
    LOAD(pGetShaderiv, "glGetShaderiv");
    LOAD(pGetShaderLog, "glGetShaderInfoLog");
    LOAD(pDeleteShader, "glDeleteShader");

    LOAD(pCreateProgram, "glCreateProgram");
    LOAD(pAttachShader, "glAttachShader");
    LOAD(pLinkProgram, "glLinkProgram");
    LOAD(pGetProgramiv, "glGetProgramiv");
    LOAD(pGetProgramLog, "glGetProgramInfoLog");
    LOAD(pUseProgram, "glUseProgram");
    LOAD(pDeleteProgram, "glDeleteProgram");

    LOAD(pGetUniformLocation, "glGetUniformLocation");
    LOAD(pUniform1i, "glUniform1i");
    LOAD(pUniform3f, "glUniform3f");

#undef LOAD
    return ok;
}

// ==========================================
// 2. Phong 光照着色器
// ==========================================

const char* vertexShaderSource = R"(
#version 120

varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec3 clothRestPosition;
varying vec3 trainDu,trainDv;
varying vec2 textureCoord;
varying vec4 shadowCoord;
uniform int materialKind;
uniform float sceneTime;
uniform vec3 cloudParams;
uniform float clothTime;
uniform float spiritTime;
uniform float spiritEnabled;
uniform float clothStrength;
uniform int clothMotionEnabled;
uniform int reflectionPass;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform mat4 shadowFromEye;
varying vec3 modelPos;

// Analytic basis and slope for the biquartic displacement patch.
void trainBasis(float t,out float b[5],out float d[5]){
    float r=1.0-t;
    float a=r*r*r,c=3.0*t*r*r,e=3.0*t*t*r,f=t*t*t;
    b[0]=r*a;b[1]=t*a+r*c;b[2]=t*c+r*e;b[3]=t*e+r*f;b[4]=t*f;
    d[0]=-4.0*a;d[1]=4.0*(a-c);d[2]=4.0*(c-e);d[3]=4.0*(e-f);d[4]=4.0*f;
}
void main()
{
    vec4 local=gl_Vertex;
    modelPos=gl_Vertex.xyz;
    trainDu=vec3(0.0);trainDv=vec3(0.0);
    if(materialKind==3){
        local.x+=.025*sin(local.y*4.0+sceneTime*1.1)*sin(local.z*3.0);
        local.z+=.035*sin(local.y*3.0-sceneTime*.8);
    }
    if(materialKind==5)local.x+=.04*sin(sceneTime*.8+local.z*.9+local.x)*clamp(gl_MultiTexCoord0.y,0.0,1.0);
    // Keep water on its authored plane; small ripples are shading-only.
    if(materialKind==8 || materialKind==9){
        float along=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
        // Red ribbon has one fixed end. Gold pibo is a loop with two fixed seams.
        float pin=materialKind==8?along*along:pow(sin(3.14159265*along),2.0);
        // The silk leads the radiance by 0.7 s: cloth moves, light follows.
        float phase=1.2566370614*(spiritTime+.7);
        float amplitude=materialKind==8?.022:.012;
        local.xyz+=spiritEnabled*pin*amplitude*vec3(0.0,sin(phase),.55*sin(phase));
    }else if(clothMotionEnabled==1 && materialKind>=26 && materialKind<=29){
        float t=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
        float u=clamp((gl_MultiTexCoord0.x-.18)/.64,0.0,1.0);
        float phase=.73*clothTime-5.4*t+.32*float(materialKind-26);
        float free=t*t*smoothstep(0.0,.9,clothTime)*clothStrength*mix(1.0,2.1,smoothstep(.23,.95,t));
        // The fitted seam stays pinned; free tips lift and turn across the wind.
        local.xyz+=free*3.10*vec3(.023*sin(phase-.5),.050*sin(phase)+.020*(u-.5)*sin(phase-.8),
                            .058*sin(phase+1.1)+.030*(u-.5)*sin(phase));
    }else if(clothMotionEnabled==1 && materialKind>=20 && materialKind<=25){
        vec2 uv=gl_MultiTexCoord0.xy;
        float t=clamp((.88-uv.y)/.75,0.0,1.0);
        float u=clamp((uv.x-.18)/.64,0.0,1.0);
        float free=t*t*smoothstep(0.0,.9,clothTime)*clothStrength*mix(1.0,2.1,smoothstep(.23,.95,t));
        float phase=.73*clothTime-5.4*t+float(materialKind-20)*.41;
        // All free layers share a travelling wind, with small delays in depth.
        float sleeveScale=(materialKind==20||materialKind==21)?.18:1.0;
        local.xyz+=free*3.30*sleeveScale*vec3(-.027*sin(phase-.4),
            .054*sin(phase)+.022*sin(phase*.71+u*3.14),
            .104*sin(phase+1.2)+.042*(2.0*u-1.0)*sin(phase-.5));
    }else if(clothMotionEnabled==1 && materialKind==19){
        // The gauze is fastened along uv.y=0 only. Sides are free, so the hem can
        // drift while the brow edge stays welded to the hair. Motion is biased
        // outward (+z), away from the face, and never crosses it.
        vec2 uv=gl_MultiTexCoord0.xy;
        float drop=clamp(uv.y,0.0,1.0);
        float pin=smoothstep(0.0,.30,drop);pin*=pin;
        float wind=min(clothStrength,1.6)*smoothstep(0.0,.9,clothTime);
        float phase=1.2566370614*clothTime;              // 5 s at 60 fps
        float across=6.2831853*uv.x;
        float slow=sin(phase-2.6*drop+across);
        float echo=sin(phase*1.37+3.1*drop-across*.6);
        float sway=pin*wind;
        local.x+=sway*(.011*slow+.004*echo);
        local.y+=sway*(.004*sin(phase-.8+2.0*drop));
        local.z+=sway*(.006+.004*echo+.003*slow);
    }else if(clothMotionEnabled==1 && materialKind==12){
        vec2 uv=gl_MultiTexCoord0.xy;
        float across=clamp((uv.x-.18)/.64,0.0,1.0);
        float along=clamp((.88-uv.y)/.75,0.0,1.0);
        float u[5],du[5],v[5],dv[5];
        trainBasis(across,u,du);trainBasis(along,v,dv);
        vec3 offset=vec3(0.0),dAcross=vec3(0.0),dAlong=vec3(0.0);
        for(int j=2;j<5;++j){
            float s=float(j)*.25;
            for(int i=0;i<5;++i){
                float w=float(i)*.25;
                float wave=.77*clothTime-7.2*s-1.85*w;
                float echo=.53*clothTime-3.5*s+1.7*w;
                vec3 handle=s*s*vec3(-.029*sin(wave-.65),
                    .049+.034*sin(wave+1.0)+.009*(2.0*w-1.0)*sin(echo),
                    .022+.084*sin(wave)+.024*sin(echo));
                // Increase the travelling wave, keeping the steady lift and pinned control rows.
                handle=handle*vec3(1.8,2.0,1.9)-s*s*vec3(0.0,.049,.0198);
                offset+=handle*u[i]*v[j];
                dAcross+=handle*du[i]*v[j];
                dAlong+=handle*u[i]*dv[j];
            }
        }
        float wind=clothStrength*smoothstep(0.0,.9,clothTime);
        local.xyz+=offset*wind*3.0;
        trainDu=mat3(gl_ModelViewMatrix)*dAcross*(wind*3.0/.64);
        trainDv=mat3(gl_ModelViewMatrix)*dAlong*(-wind*3.0/.75);
    }else if(clothMotionEnabled==1 && materialKind>=8 && materialKind<=16){
        // Ribbon and gold edging share the same displacement field.
        float s;
        vec3 offset;
        if(materialKind==8){
            s=clamp((-local.x-.08)/1.06,0.0,1.0);
            float phase=.86*clothTime-6.15*s;
            float echo=.57*clothTime-3.8*s+.6;
            offset=vec3(.023*sin(phase+.55),.080*sin(phase)+.023*sin(echo),
                        .075*sin(phase+1.24)+.026*sin(echo+1.6));
        }else if(materialKind==9){
            // The loop attaches near the shoulder and returns toward the waist.
            float along=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
            s=sin(3.14159265*along);
            float phase=.63*clothTime-6.0*along;
            offset=vec3(.058*sin(phase-.55),.075*sin(phase),.120*sin(phase+1.38));
        }else if(materialKind==15){
            // Only the lower inner skirt breathes; upper layers stay anchored.
            s=clamp((.60-local.y)/.59,0.0,1.0);
            float phase=.64*clothTime-2.1*s+7.5*local.x+5.4*local.z;
            offset=vec3(.021*sin(phase+.65),.014+.013*sin(phase),.021*sin(phase+1.25));
        }else if(materialKind==14){
            s=clamp((1.055-local.y)/.19,0.0,1.0);
            float phase=.91*clothTime-3.0*s+1.7;
            offset=vec3(.019*sin(phase),.008*sin(phase+.7),.026*sin(phase+1.1));
        }else{
            // The folded shells use V as a shared root-to-tip coordinate on both sides.
            s=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
            if(materialKind==11){
                float u=clamp((gl_MultiTexCoord0.x-.18)/.64,0.0,1.0);
                float edge=2.0*u-1.0;
                float freeEdge=smoothstep(.54,1.0,s)*edge*abs(edge);
                // The broad upper panel trails back. Its hem turns across the flow.
                float phase=.66*clothTime-6.2*s+2.3*u+.45;
                float roll=freeEdge*sin(.49*clothTime-4.1*s+.8);
                offset=vec3(.008*sin(phase-.4),
                            .017+.014*sin(phase+.9)+.013*roll,
                            .030*sin(phase)+.012*roll);
            }else if(materialKind==13){
                float u=clamp((gl_MultiTexCoord0.x-.18)/.64,0.0,1.0);
                float phase=.69*clothTime-4.2*s+1.45*u+2.1;
                float curl=smoothstep(.56,1.0,s)*(2.0*u-1.0)*sin(phase+.9);
                offset=vec3(.014*sin(phase)+.007*curl,.014*sin(phase+.4)+.006*curl,
                            .021*sin(phase+1.5));
            }else{
                float u=clamp((gl_MultiTexCoord0.x-.18)/.64,0.0,1.0);
                float phase=.73*clothTime-5.4*s+1.8*u+2.4;
                float returnWave=.51*clothTime-3.2*s-2.6*u;
                if(materialKind==16){
                    float fringe=clamp((gl_MultiTexCoord0.x-.82)/.13,0.0,1.0);
                    phase-=.24*fringe;
                    returnWave-=.20*fringe;
                }
                // The sleeve sways mainly in depth. At u=1 its fringe shares the seam.
                float roll=smoothstep(.52,1.0,s)*(2.0*u-1.0)*sin(returnWave+.65);
                offset=vec3(.012*sin(phase),.021*sin(phase)+.006*sin(returnWave)+.010*roll,
                            .041*sin(phase+1.25)+.010*sin(returnWave+.9));
            }
        }
        float pin=smoothstep(.04,1.0,s);
        // 袖子的模型接缝经不起大位移，单独压回来，避免露馅。
        float sleeveScale=(materialKind==10||materialKind==13||materialKind==16)?.22:1.0;
        float breeze=materialKind==15?1.0:1.38;
        // Keep the cuffs and tucked seams steady. Amplify only the parts in the wind.
        float freeEnd=smoothstep(.23,.95,s);
        vec3 gain=vec3(1.0);
        if(materialKind==10||materialKind==16)gain=vec3(2.25,3.10,2.80);
        else if(materialKind==11)gain=vec3(2.18,2.85,2.63);
        else if(materialKind==13)gain=vec3(2.19,2.77,2.63);
        else if(materialKind==8||materialKind==9)gain=vec3(2.28,2.81,2.70);
        local.xyz+=offset*mix(vec3(1.0),gain,freeEnd)*pin*pin*breeze*sleeveScale
                    *5.20*clothStrength*smoothstep(0.0,.9,clothTime);
    }
    if(materialKind==38){
        // Shore reeds: the base stays planted while the tips sway in the breeze.
        float h=clamp(local.y/.55,0.0,1.0);
        float ph=sceneTime*1.15+local.x*2.6+local.z*1.9;
        local.x+=.062*h*sin(ph);
        local.z+=.038*h*sin(ph+1.7);
    }else if(materialKind==37||materialKind==39){
        // Hanging ornaments: higher parts stay closer to the head/body, the lower
        // ends (earrings, jade pendants, waist pearls) drift gently.
        float root=clamp((1.55-local.y)/.85,0.0,1.0);
        root=.15+.85*root;
        float ph=sceneTime*.95+local.x*4.0+local.z*3.0;
        local.x+=.034*root*sin(ph);
        local.z+=.023*root*sin(ph+1.6);
    }
    vec4 position = gl_ModelViewMatrix * local;
    if(reflectionPass==1){
        float depth=max(-(dot(position.xyz,worldUpEye)+cameraHeight),0.0);
        position.x+=(.008+.023*depth)*sin(depth*33.0+sceneTime*.9+position.z*.25);
    }

    shadowCoord=shadowFromEye*position;
    eyePosition = position.xyz;
    clothRestPosition = (materialKind>=8 && materialKind<=17)||(materialKind>=26&&materialKind<=29)
        ?(gl_ModelViewMatrix * gl_Vertex).xyz:position.xyz;
    eyeNormal = normalize(gl_NormalMatrix * gl_Normal);
    textureCoord = gl_MultiTexCoord0.xy;

    gl_Position = gl_ProjectionMatrix * position;
}
)";

const char* fragmentShaderSource = R"(
#version 120
uniform sampler2D texture0;
uniform sampler2D normalMap;
uniform sampler2DShadow shadowMap;
uniform int renderDepth;
uniform int veilPass;
uniform int veilProbe;
uniform int shadowsEnabled;
uniform int reflectionPass;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform vec3 lightPositionEye;
uniform vec3 goddessLightEye;
uniform int goddessDraw;
uniform float spiritTime;
uniform float spiritEnabled;
uniform vec3 baseColor;
uniform int useTexture;
uniform int materialKind;
uniform float sceneTime;
uniform vec3 cloudParams;
uniform vec3 goddessPosition;
uniform vec3 footPosition;
uniform vec3 wakeTrail[16];
uniform vec3 boatPosition;
uniform vec3 goddessVelocity;
uniform vec3 boatVelocity;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 camBack;
uniform vec3 cloudCameraWorld;
varying vec3 modelPos;
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec3 clothRestPosition;
varying vec3 trainDu,trainDv;
varying vec2 textureCoord;
varying vec4 shadowCoord;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1.0,0.0)),f.x),mix(hash(i+vec2(0.0,1.0)),hash(i+vec2(1.0,1.0)),f.x),f.y);}
float shadowTap(vec3 sc,vec2 offset,float bias){
    vec2 uv=sc.xy+offset*vec2(2.26,3.06)/4096.0;
    if(uv.x<=0.0||uv.x>=1.0||uv.y<=0.0||uv.y>=1.0)return 1.0;
    return shadow2D(shadowMap,vec3(uv,sc.z-bias)).r;
}
// Integrate only the air between the eye and the visible surface.
// The same field is used for sky and geometry; clouds never write depth.
bool cloudSlab(float origin,float direction,float low,float high,inout float enter,inout float leave){
    if(abs(direction)<.00001)return origin>=low && origin<=high;
    float a=(low-origin)/direction,b=(high-origin)/direction;
    enter=max(enter,min(a,b));leave=min(leave,max(a,b));
    return leave>enter;
}
float valleyCloud(vec3 ray,float surfaceDistance){
    float opticalDepth=0.0;
    for(int layer=0;layer<4;++layer){
        float k=float(layer);
        float centerZ=layer==0?-28.0:(layer==2?-43.5:-29.2);
        float halfDepth=layer==2?.20:.28;
        float lowX=layer==0?-18.0:(layer==1?0.0:(layer==2?-6.0:12.0));
        float highX=layer==0?-2.0:(layer==1?14.0:(layer==2?12.0:29.0));
        float begin=0.0,end=200.0;
        bool hit=cloudSlab(cloudCameraWorld.x,ray.x,lowX,highX,begin,end);
        hit=cloudSlab(cloudCameraWorld.y,ray.y,.65,9.5,begin,end)&&hit;
        hit=cloudSlab(cloudCameraWorld.z,ray.z,centerZ-halfDepth,centerZ+halfDepth,begin,end)&&hit;
        if(hit && surfaceDistance>begin){
            // Grid depends on the full cloud box, never on the occluding mesh.
            float stepLength=(end-begin)/64.0;
            float stop=min(end,surfaceDistance);
            float ct=spiritTime;
            for(int sampleIndex=0;sampleIndex<64;++sampleIndex){
                float cell=begin+float(sampleIndex)*stepLength;
                if(cell>=stop)break;
                float covered=min(stepLength,stop-cell);
                vec3 p=cloudCameraWorld+ray*(cell+.5*stepLength);
                // Slow drift on every octave, plus a fine strand field that breaks the
                // edges into moving wisps instead of a solid slab.
                float broad=noise(vec2(p.x*.37+k*3.1+ct*.03,p.z*.17-ct*.02));
                float detail=noise(vec2(p.x*1.05-k*2.3-ct*.12,p.z*.39+ct*.075));
                float wisp=noise(vec2(p.x*2.9+k*1.7+ct*.36,p.z*1.2-ct*.25));
                float x=p.x;
                float crest;
                if(layer==0){
                    crest=3.6+.4*smoothstep(-18.0,-16.0,x)-.6*smoothstep(-16.0,-14.0,x)
                        +.7*smoothstep(-14.0,-12.0,x)+.7*smoothstep(-12.0,-10.0,x)
                        -.4*smoothstep(-10.0,-8.0,x)-.6*smoothstep(-8.0,-6.0,x)
                        -.3*smoothstep(-6.0,-4.0,x)-.1*smoothstep(-4.0,-2.0,x);
                }else if(layer==1){
                    crest=3.6+.8*smoothstep(0.0,2.0,x)+.8*smoothstep(2.0,4.0,x)
                        +.9*smoothstep(4.0,6.0,x)+.1*smoothstep(6.0,8.0,x)
                        -.4*smoothstep(8.0,10.0,x)-.3*smoothstep(10.0,12.0,x)-.2*smoothstep(12.0,14.0,x);
                }else if(layer==2){
                    crest=5.6+.7*smoothstep(-6.0,-3.0,x)+.9*smoothstep(-3.0,0.0,x)
                        +.9*smoothstep(0.0,3.0,x)+.5*smoothstep(3.0,6.0,x)
                        +.2*smoothstep(6.0,9.0,x)+.2*smoothstep(9.0,12.0,x);
                }else{
                    crest=5.7+.65*smoothstep(12.0,18.0,x)+1.05*smoothstep(18.0,25.0,x)
                        +(noise(vec2(x*.42,8.7))-.5)*.65;
                }
                if(layer==0)crest-=.32;
                crest+=(broad-.5)*.32+(detail-.5)*.16;
                float taper=smoothstep(lowX,lowX+3.0,x)*(1.0-smoothstep(highX-3.0,highX,x));
                float thickness=(.35+1.10*broad)*taper;
                float top=1.0-smoothstep(crest-.08,crest+.13,p.y);
                float foot=smoothstep(crest-thickness-.60,crest-thickness+.12,p.y);
                float sides=taper;
                float along=1.0-smoothstep(halfDepth*.4,halfDepth,abs(p.z-centerZ));
                float folds=.40+.60*broad;
                // Per-layer slow fade so each bank of cloud swells and thins on its own
                // rhythm; the strand field softens the rim as it goes.
                float fade=smoothstep(.25,.95,.5+.5*sin(ct*.37+float(layer)*2.1+detail*1.3));
                float strands=.5+.5*wisp;
                opticalDepth+=top*foot*sides*along*folds*strands*fade*covered*1.5;
            }
        }
    }
    return 1.0-exp(-opticalDepth);
}
vec3 throughValleyCloud(vec3 background,vec3 ray,float surfaceDistance){
    float a=valleyCloud(ray,surfaceDistance);
    return mix(background,vec3(.91,.935,.95),a);
}
// Elliptical reflection from aligned silk threads. Two widths give the
// travelling highlight a soft shoulder without flattening the entire fold.
float silkHighlight(vec3 N,vec3 warp,vec3 weft,vec3 V,vec3 light,vec2 profile){
    vec3 halfSum=light+V;
    vec3 H=halfSum*inversesqrt(max(dot(halfSum,halfSum),1e-8));
    float facing=dot(N,H);
    vec2 tilt=vec2(dot(warp,H),dot(weft,H))/max(facing,.25);
    vec2 slope=tilt*tilt;
    float peak=exp(-dot(slope,profile));
    float shoulder=exp(-dot(slope,profile*vec2(.48,.35)));
    return (.63*peak+.37*shoulder)*sqrt(max(dot(N,light),0.0))
           *smoothstep(.08,.38,facing);
}
// Delicate scrolls in a narrow band above the gold-thread hem.
vec2 gauzeTrim(vec2 uv){
    vec2 edge=min(uv,1.0-uv);
    float sides=1.0-smoothstep(.004,.016,edge.x);
    float bottom=1.0-smoothstep(.005,.024,1.0-uv.y);
    float border=max(sides*smoothstep(.05,.27,uv.y),bottom);
    vec2 p=vec2(fract(uv.x*5.0)-.5,(uv.y-.84)*4.8);
    float arc=abs(length(p*vec2(1.0,1.7))-.24);
    float curl=abs(length((p-vec2(.17,.018))*vec2(1.0,1.5))-.091);
    float band=smoothstep(.77,.80,uv.y)*(1.0-smoothstep(.90,.93,uv.y));
    float motif=(1.0-smoothstep(.010,.034,min(arc,curl)))*band
                *smoothstep(.04,.14,edge.x);
    return vec2(border,motif);
}
void main(){
    gl_FragDepth=gl_FragCoord.z;
    // Environment identifiers 30-32 are separate from character materials 8-29.
    if(materialKind==30 && renderDepth==0){
        vec2 uv=gl_FragCoord.xy/max(cloudParams.yz,vec2(1.0));
        vec2 ndc=uv*2.0-1.0;
        float aspect=max(cloudParams.y/max(cloudParams.z,1.0),.5);
        vec3 rd=normalize(camRight*(ndc.x*aspect*.4142)+camUp*(ndc.y*.4142)-camBack);
        vec3 sunDir=normalize(vec3(-.12,.16,-1.0));
        float ang=acos(clamp(dot(rd,sunDir),-1.0,1.0));
        float lift=smoothstep(0.0,.5,rd.y);
        vec3 sky=mix(vec3(.97,.66,.46),vec3(.99,.76,.58),lift);
        float disc=1.0-smoothstep(.036,.040,ang);
        float below=dot(sunDir,camUp)-dot(rd,camUp);
        float cut=step(below,.012);
        disc*=cut;
        float halo=(1.0-smoothstep(.040,.055,ang))*step(below,.014);
        sky=mix(sky,vec3(.99,.80,.64),halo*.12);
        sky=mix(sky,vec3(.99,.85,.56),disc*.82);
        sky=throughValleyCloud(sky,rd,160.0);
        gl_FragColor=vec4(sky,1.0);
        return;
    }
    vec4 color=vec4(baseColor,1.0);
    if(useTexture==1)color*=texture2D(texture0,textureCoord);
    if(color.a<0.44)discard;
    float veilAlpha=1.0;
    bool airy=materialKind>=20 && materialKind<=25;
    bool drape=materialKind>=26 && materialKind<=29;
    bool isVeil=materialKind==9||materialKind==16||materialKind==19||airy||drape;
    if(isVeil){
        float u=clamp((textureCoord.x-.18)/.64,0.0,1.0);
        float edge=abs(u*2.0-1.0);
        if(drape){
            float edgeFade=smoothstep(0.0,.20,min(u,1.0-u));
            float t=clamp((.88-textureCoord.y)/.75,0.0,1.0);
            float panel=materialKind==26?.76:(materialKind==27?.80:(materialKind==28?.69:.74));
            float falloff=smoothstep(.55,.98,t);
            float hemTurn=sin(6.2831853*(u+.13*float(materialKind-26)));
            float bodyAlpha=mix(panel,panel*.74,falloff*(.42+.18*hemTurn));
            float clothCoverage=mix(.58,.96,edgeFade);
            veilAlpha=bodyAlpha*clothCoverage;
            vec2 yarn=textureCoord*vec2(82.0,118.0);
            vec2 resolve=1.0-smoothstep(vec2(.12),vec2(.42),fwidth(yarn));
            color.rgb*=1.0+.009*dot(sin(6.2831853*yarn)*resolve,vec2(.5));
        }else if(airy){
            float t=clamp((.88-textureCoord.y)/.75,0.0,1.0);
            float selvedge=1.0-smoothstep(.73,.998,edge);
            float density=(materialKind==20||materialKind==21||materialKind==25)?.27:.34;
            float hem=1.0-.50*smoothstep(.84,1.0,t);
            veilAlpha=mix(.035,density,selvedge)*hem;
            // Subtle warp threads fade with distance instead of aliasing.
            vec2 yarn=textureCoord*vec2(96.0,132.0);
            vec2 resolve=1.0-smoothstep(vec2(.12),vec2(.44),fwidth(yarn));
            color.rgb*=1.0+.012*dot(sin(6.2831853*yarn)*resolve,vec2(.5));
        }else if(materialKind==9){
            veilAlpha=mix(.68,.36,smoothstep(.13,1.0,edge));
            if(veilProbe==1)veilAlpha=.14;
        }else if(materialKind==19){
            vec2 uv=clamp(textureCoord,0.0,1.0);
            float side=min(uv.x,1.0-uv.x);
            float hem=min(uv.y,1.0-uv.y);
            float fade=smoothstep(0.0,.14,side)*smoothstep(0.0,.15,hem);
            vec2 weave=uv*vec2(34.0,29.0);
            vec2 resolve=1.0-smoothstep(vec2(.13),vec2(.47),fwidth(weave));
            float thread=dot(sin(6.2831853*weave)*resolve,vec2(.009));
            vec2 trim=gauzeTrim(uv);
            veilAlpha=mix(.46,.86,fade)+thread*fade+.35*trim.x+.08*trim.y;
        }else{
            float fringe=clamp((textureCoord.x-.82)/.13,0.0,1.0);
            veilAlpha=mix(.64,.28,smoothstep(.08,1.0,fringe));
        }
    }
    if(renderDepth==1){
        // The opaque layers still cast shadows; thin veils do not stamp dark silhouettes.
        if(isVeil)discard;
        gl_FragColor=vec4(1.0);return;
    }
    if(reflectionPass==0 && ((veilPass==0 && isVeil)||(veilPass==1 && !isVeil)))discard;
    if(materialKind==6){gl_FragColor=vec4(.035,.065,.06,.24);return;}
    vec3 N=normalize(eyeNormal);if(!gl_FrontFacing)N=-N;
    float sunBand=0.0;
    bool silk=(materialKind>=8 && materialKind<=17)||airy||drape;
    bool ivory=materialKind==11||materialKind==12||materialKind==15||(materialKind>=22&&materialKind<=24)||(materialKind>=27&&materialKind<=29);
    bool sleeve=materialKind==10||materialKind==13||materialKind==17||materialKind==20||materialKind==21||materialKind==25||materialKind==26;
    vec3 warp=vec3(0.0),weft=vec3(0.0);
    vec3 sheenNormal=N;
    float valley=0.0,overlap=0.0,grainRoughness=1.0;
    if(silk){
        vec3 restN=N;
        vec3 normalDx=dFdx(restN),normalDy=dFdy(restN);
        vec3 dxRest=dFdx(clothRestPosition),dyRest=dFdy(clothRestPosition);
        vec3 dx=dFdx(eyePosition),dy=dFdy(eyePosition);
        vec2 uvDx=dFdx(textureCoord),uvDy=dFdy(textureCoord);
        if(materialKind==12 && reflectionPass==0){
            dx=dxRest+trainDu*uvDx.x+trainDv*uvDx.y;
            dy=dyRest+trainDu*uvDy.x+trainDv*uvDy.y;
        }
        vec3 restCross=cross(dxRest,dyRest),movedCross=cross(dx,dy);
        float restArea=length(restCross),movedArea=length(movedCross);
        if(min(restArea,movedArea)>1e-8){
            vec3 restFace=restCross/restArea,movedFace=movedCross/movedArea;
            // Carry the original smooth normal through the local deformation
            // with the inverse transpose of the rest-to-current surface frame.
            vec3 movedN=dot(restN,dxRest)*cross(dy,movedFace)
                      +dot(restN,dyRest)*cross(movedFace,dx)
                      +dot(restN,restFace)*movedCross;
            vec3 normalShift=normalize(movedN)-restN;
            // Bound face-to-face changes at the fast-moving hem. The silk
            // reflection tracks more of the rotation than the diffuse base.
            normalShift*=min(1.0,.33/max(length(normalShift),1e-5));
            N=normalize(restN+.46*normalShift);
            sheenNormal=normalize(restN+(materialKind==12?1.0:.81)*normalShift);

            vec3 across=dxRest*uvDy.y-dyRest*uvDx.y;
            across-=restN*dot(restN,across);
            if((ivory||sleeve)&&dot(across,across)>1e-16){
                across=normalize(across);
                vec2 alongScreen=vec2(dot(across,cross(dyRest,restFace)),
                                      dot(across,cross(restFace,dxRest)))/restArea;
                vec3 slope=normalDx*alongScreen.x+normalDy*alongScreen.y;
                // Negative curvature marks a hollow, as opposed to a lit ridge.
                valley=smoothstep(.30,3.70,-dot(slope,across));
            }
        }
        warp=dy*uvDx.x-dx*uvDy.x;
        warp-=N*dot(N,warp);
        if(dot(warp,warp)<1e-16)warp=worldUpEye-N*dot(N,worldUpEye);
        if(dot(warp,warp)<1e-16){
            vec3 axis=abs(N.z)<.90?vec3(0.0,0.0,1.0):vec3(1.0,0.0,0.0);
            warp=cross(N,axis);
        }
        warp=normalize(warp);
        weft=cross(N,warp);

        if(ivory||sleeve){
            // The grid stays attached to UVs and fades below pixel resolution.
            vec2 threads=textureCoord*vec2(78.0,106.0);
            vec2 coverage=1.0-smoothstep(vec2(.11),vec2(.36),fwidth(threads));
            vec2 bend=.085*sin(textureCoord.yx*vec2(27.0,31.0));
            vec2 yarn=sin(6.2831853*(threads+bend))*coverage;
            float slub=sin(6.2831853*(textureCoord.y*9.0+.18*sin(textureCoord.x*21.0)));
            float fiber=sleeve?.010:.008;
            sheenNormal=normalize(sheenNormal+fiber*(yarn.x*weft+.65*yarn.y*warp));
            grainRoughness=1.0+.042*slub+.020*yarn.x*yarn.y;
            color.rgb*=1.0+.004*yarn.x+.003*yarn.y;
            if(materialKind>=10 && materialKind<=13){
                float u=clamp((textureCoord.x-.18)/.64,0.0,1.0);
                float lengthUV=clamp((.88-textureCoord.y)/.75,0.0,1.0);
                // The existing loft seams lie beneath the neighboring layers.
                overlap=(1.0-smoothstep(.03,.37,lengthUV))*mix(.47,1.0,smoothstep(.33,.93,u));
                valley*=1.0-.30*smoothstep(.73,.99,lengthUV);
            }
        }
    }
    if(sleeve)color.rgb*=vec3(.88,.96,1.10);
    if(materialKind==1){
        vec2 flow=textureCoord*10.0;
        float away=smoothstep(2.0,-10.0,flow.y);
        color.rgb=mix(vec3(.45,.584,.61),vec3(.53,.654,.66),away);
        vec2 fromSun=flow-vec2(-1.1,-2.4);
        float ripplePhase=flow.y*20.0+5.5*noise(vec2(flow.x*.72,flow.y*2.1))
            +1.35*sin(flow.y*5.7)+1.05*sin(flow.x*2.3+flow.y*1.4);
        float rippleWidth=max(fwidth(ripplePhase),.05);
        float ripple=smoothstep(.42-rippleWidth,.82+rippleWidth,sin(ripplePhase));
        float fragments=smoothstep(.32,.65,noise(vec2(flow.x*5.5,flow.y*8.2)));
        float streak=.20+.80*ripple*fragments;
        float spread=.95+.065*abs(fromSun.y);
        sunBand=exp(-fromSun.x*fromSun.x/(spread*spread)-fromSun.y*fromSun.y/40.0)*streak;
        color.rgb*=.99+.025*noise(vec2(flow.x*.8,flow.y*7.0));
)"
R"(
        // Quiet travelling ripples at the actual foot contact, with a soft local envelope.
        vec2 rippleOffset=flow-footPosition.xz;
        float radius=length(rippleOffset);
        float envelope=(1.0-smoothstep(.25,2.1,radius))*smoothstep(.06,.24,radius);
        float phase=18.0*radius-1.2566370614*spiritTime;
        vec2 slope=spiritEnabled*.010*envelope*cos(phase)*rippleOffset/max(radius,.06);
        vec3 axisX=vec3(camRight.x,camUp.x,camBack.x);
        vec3 axisZ=vec3(camRight.z,camUp.z,camBack.z);
        N=normalize(N-axisX*slope.x-axisZ*slope.y);
        float ringWidth=max(fwidth(phase)*1.4,.12);
        float ring=smoothstep(.72-ringWidth,.96+ringWidth,sin(phase));
        float broken=.68+.32*sin(atan(rippleOffset.y,rippleOffset.x)*3.0+radius*2.0);
        color.rgb+=spiritEnabled*envelope*vec3(.034,.041,.039)*(ring-.22)*broken;
        color.rgb+=vec3(.35,.42,.44)*dot(slope,slope)*10.0;

    }
)"
R"(
    if(materialKind==2){
        float patch=.5+.5*sin(textureCoord.x*2.0+sin(textureCoord.y*1.8))*cos(textureCoord.y*2.4);
        color.rgb=mix(color.rgb,vec3(.64,.62,.58),.80);
        color.rgb*=mix(vec3(.92,.90,.86),vec3(1.02,1.00,.96),patch);
        color.rgb*=.97+.06*hash(floor(textureCoord*28.0));
        vec2 ground=textureCoord/0.4;
        float shore=-3.5+.65*sin(ground.y*.16)+.35*sin(ground.y*.53+.8)+.12*sin(ground.y*1.4)-ground.x;
        color.rgb=mix(color.rgb,vec3(.58,.60,.62),.22*exp(-max(shore,0.0)*1.4));
        float shade=exp(-dot((ground-vec2(-8.0,-3.5))/vec2(2.6,3.2),(ground-vec2(-8.0,-3.5))/vec2(2.6,3.2)));
        color.rgb*=1.0-.08*shade;
        vec2 contact=(ground-vec2(-4.3,1.0))/vec2(1.1,.85);
        color.rgb*=1.0-.10*exp(-dot(contact,contact));
        // Tangent-space normal mapping: derive a cotangent frame from UV/position derivatives.
        vec3 dp1=dFdx(eyePosition),dp2=dFdy(eyePosition);
        vec2 duv1=dFdx(textureCoord),duv2=dFdy(textureCoord);
        vec3 dp2perp=cross(dp2,N),dp1perp=cross(N,dp1);
        vec3 T=dp2perp*duv1.x+dp1perp*duv2.x;
        vec3 B=dp2perp*duv1.y+dp1perp*duv2.y;
        float inv=inversesqrt(max(max(dot(T,T),dot(B,B)),1e-8));
        vec3 mapped=texture2D(normalMap,textureCoord).xyz*2.0-1.0;
        mapped.xy*=.55;N=normalize(mat3(T*inv,B*inv,N)*normalize(mapped));
    }
    if(materialKind==4){
        float layer=smoothstep(.30,.52,color.b);
        color.rgb=mix(vec3(.50,.58,.64),vec3(.68,.74,.77),layer);
        color.rgb*=.93+.12*noise(textureCoord*4.5);
    }
    if(materialKind==0 || materialKind==3 || materialKind==7){
        float warm=clamp((color.r-color.b-.06)/.20,0.0,1.0)*smoothstep(.55,.82,color.r);
        color.rgb=mix(color.rgb,vec3(min(color.r*1.03,1.0),min(color.g*1.01,.96),mix(color.b,.86,.75)),warm*.65);
        float leaf=smoothstep(.04,.14,color.g-color.b)*smoothstep(.04,.12,color.g-color.r);
        float teal=smoothstep(.0,.08,color.g-color.r)*smoothstep(.22,.50,color.g)*(1.0-leaf);
        color.rgb=mix(color.rgb,vec3(color.r*.74,min(color.g*1.06,.82),min(color.b+.08,color.g)),teal*.65);
        color.rgb=mix(color.rgb,vec3(color.r*.96,color.g*.66,color.b*.76),leaf*.72);
    }
    if(materialKind==5)color.rgb=mix(color.rgb,vec3(color.r*.92,color.g*.64,color.b*.72),.6);
    if((materialKind==0||materialKind==36) && useTexture==0){
        float peak=max(color.r,color.g);
        float lift=smoothstep(.22,.30,peak)*(1.0-smoothstep(.42,.70,peak));
        color.rgb=mix(color.rgb,color.rgb*.5+vec3(.46,.44,.39),lift);
    }
)"
R"(
    float ridge=0.0;
    // All mountain draws currently use this explicit tint; other draws use white.
    // This marker is independent of camera distance and texture mip selection.
    if(materialKind==0 && distance(baseColor,vec3(.84,.91,1.0))<.015)ridge=1.0;
    float ridgeShade=0.0;
    // Art-directed front-left key; the sunset disc is a background accent.
    // Keep the key fixed in world space as the viewer moves.
    vec3 sunWorld=normalize(vec3(-.48,.48,.74));
    vec3 sunEye=vec3(dot(camRight,sunWorld),dot(camUp,sunWorld),dot(camBack,sunWorld));
    vec3 L=normalize(sunEye),V=normalize(-eyePosition),R=reflect(-L,N);
    vec3 fillWorld=normalize(vec3(.25,.70,.55));
    vec3 skyFill=vec3(dot(camRight,fillWorld),dot(camUp,fillWorld),dot(camBack,fillWorld));
    float diffuse=max(dot(N,L),0.0);
    float shininess=materialKind==1?28.0:(materialKind==7?40.0:22.0);
    float specular=diffuse>0.0?pow(max(dot(R,V),0.0),shininess):0.0;
    float visibility=1.0;
    vec3 sc=shadowCoord.xyz/shadowCoord.w;
    if(shadowsEnabled==1 && reflectionPass==0 && materialKind!=1 && materialKind!=4 && sc.x>0.0 && sc.x<1.0 && sc.y>0.0 && sc.y<1.0 && sc.z>0.0 && sc.z<1.0){
        float bias=silk?max(.0008,.0018*(1.0-diffuse)):max(.0022,.0052*(1.0-diffuse));
        if(materialKind==10||materialKind==11||materialKind==12||materialKind==13)bias+=.00045;
        bias*=89.8/159.8; // Preserve world-depth offset after extending the light frustum.
        float sum=0.0;
        if(silk){
            // Preserve the tighter upstream cloth shadow filter (shadowTap uses eight texels).
            sum+=shadowTap(sc,vec2(.35,.45)*.25,bias);
            sum+=shadowTap(sc,vec2(-.45,.25)*.25,bias);
            sum+=shadowTap(sc,vec2(.25,-.35)*.25,bias);
            sum+=shadowTap(sc,vec2(-.35,-.45)*.25,bias);
            visibility=.18+.82*sum/4.0;
        }else{
        // About eight texels wide, so folded-rock edges stay soft and faces are not speckled.
        sum+=shadowTap(sc,vec2(1.05,.72),bias);
        sum+=shadowTap(sc,vec2(-1.12,.48),bias);
        sum+=shadowTap(sc,vec2(.58,-1.02),bias);
        sum+=shadowTap(sc,vec2(-.72,-.95),bias);
        visibility=.55+.45*sum/4.0;
        }
    }
    // Smoothstep keeps low-poly planes apart without driving a back face to black.
    float fold=smoothstep(.08,.82,diffuse);
    vec3 lit;
    if(silk){
        if(drape)visibility=mix(.72,1.0,visibility);
    vec3 directTint=vec3(.88,.70,.49);
        if(ivory)directTint=vec3(.50,.47,.43);
        else if(sleeve)directTint=vec3(.68,.60,.49);
        else if(materialKind==9)directTint=vec3(.67,.56,.40);
        float clothAO=1.0-.18*valley-.15*overlap;
        lit=color.rgb*(vec3(.39,.41,.43)*clothAO
            +directTint*diffuse*visibility*(1.0-.075*valley));
        lit+=color.rgb*vec3(.25,.24,.22)*max(dot(N,skyFill),0.0)*clothAO;
        // Project the moving fiber axis back onto the finely perturbed surface.
        vec3 sheenWarp=normalize(warp-sheenNormal*dot(sheenNormal,warp));
        vec3 sheenWeft=cross(sheenNormal,sheenWarp);
        vec2 strength=ivory?vec2(.48,.29):(sleeve?vec2(.54,.29):vec2(.22,.09));
        vec2 profile=ivory?vec2(4.0,17.0):(sleeve?vec2(3.1,29.0):vec2(5.0,24.0));
        if(materialKind==12){
            strength=vec2(.57,.34);
            profile=vec2(3.7,20.0);
        }else if(materialKind==15)strength*=.79;
        else if(materialKind==17)strength=vec2(.29,.15);
        else if(materialKind==10){strength=vec2(.25,.14);profile=vec2(2.4,13.0);}
        else if(drape){strength=vec2(.14,.10);profile=vec2(2.0,10.0);}
        else if(isVeil){
            strength=vec2(.145,.052);
            profile=vec2(3.2,9.0);
        }
        strength*=sleeve?vec2(.16,.10):vec2(.34,.22);
        vec3 tint=mix(color.rgb,vec3(.94,.91,.85),.42);
        if(ivory)tint=mix(color.rgb,vec3(1.0,.96,.86),.68);
        else if(sleeve)tint=mix(color.rgb,vec3(.65,.79,.82),.45);
        else if(materialKind==9)tint=mix(color.rgb,vec3(1.0,.91,.74),.48);
        float keySilk=silkHighlight(sheenNormal,sheenWarp,sheenWeft,V,L,profile*grainRoughness);
        float skySilk=silkHighlight(sheenNormal,sheenWarp,sheenWeft,V,skyFill,profile*grainRoughness);
        vec3 glint=tint*(strength.x*keySilk*visibility+strength.y*skySilk*clothAO)
                  *(1.0-.29*valley-.16*overlap);
        // Roll off the crest as it approaches display white, preserving
        // the color and separation of adjacent folds in side views.
        float headroom=max(1.0-max(lit.r,max(lit.g,lit.b)),0.0);
        float glintPeak=max(glint.r,max(glint.g,glint.b));
        lit+=glint*headroom/(headroom+glintPeak+1e-5);
        if(isVeil){
            float transmission=materialKind==9?.047:.035;
            lit+=color.rgb*transmission*smoothstep(.04,.84,-dot(N,L))*visibility;
        }
    }else if(materialKind==1){
        lit=color.rgb;
        lit=mix(lit,vec3(.98,.80,.58),sunBand*.72);
        // Character rings keep their original look. Hull rings are fewer, start
        // tight to the boat and live longer so each one reads as a clean ripple.
        float trailLight=0.0,boatLight=0.0;
        for(int i=0;i<16;++i){
            float age=wakeTrail[i].z;
            float radius=length(textureCoord*10.0-wakeTrail[i].xy);
            bool hull=i>=8;
            float width=max(fwidth(radius)*1.2,hull?.050:.035);
            float crest=(hull?.30:.25)+age*(hull?.62:.72);
            float arc=1.0-smoothstep(width,width*2.0,abs(radius-crest));
            float fade=hull?smoothstep(.0,.20,age)*(1.0-smoothstep(.55,1.90,age))
                           :smoothstep(.0,.12,age)*(1.0-smoothstep(.25,1.05,age));
            float breakup=smoothstep(hull?.20:.30,hull?.60:.65,noise((textureCoord*10.0-wakeTrail[i].xy)*5.0));
            if(hull)boatLight=max(boatLight,arc*fade*breakup);
            else trailLight=max(trailLight,arc*fade*breakup);
        }
        lit=mix(lit,vec3(.84,.76,.60),trailLight*.32);
        lit+=boatLight*vec3(.235,.240,.225);

    }else if(ridge>.45){
        vec3 mountainNormal=normalize(camRight*N.x+camUp*N.y+camBack*N.z);
        float shaped=smoothstep(-.35,.80,dot(mountainNormal,normalize(vec3(.85,.32,.25))));
        ridgeShade=1.0-shaped;
        float aerial=smoothstep(28.0,105.0,length(eyePosition));
        vec3 nearShade=vec3(.34,.43,.46),nearLight=vec3(.53,.60,.60);
        vec3 farShade=vec3(.59,.63,.63),farLight=vec3(.72,.72,.68);
        lit=mix(mix(nearShade,nearLight,shaped),mix(farShade,farLight,shaped),aerial);
    }else if(materialKind==4){
        float shaped=clamp(diffuse*.85+.1,0.0,1.0);
        shaped=shaped*.7+smoothstep(.05,.8,diffuse)*.3;
        lit=color.rgb*(vec3(.34,.38,.42)+vec3(.74,.60,.46)*shaped);
    }else if(materialKind==2){
        float shaped=diffuse*.7+fold*.3;
        lit=color.rgb*(vec3(.50,.48,.46)+vec3(.46,.44,.42)*shaped*visibility);
    }else if((materialKind==0||materialKind==36) && useTexture==0){
        float peak=max(color.r,color.g);
        float ink=1.0-smoothstep(.16,.26,peak);
        float wrap=clamp(diffuse*.7+.22,0.0,1.0);
        float shaped=wrap*.6+fold*.4;
        vec3 amb=mix(vec3(.40,.38,.34),vec3(.16,.13,.11),ink);
        vec3 key=mix(vec3(.62,.54,.42),vec3(.28,.22,.16),ink);
        lit=color.rgb*(amb+key*shaped*visibility);
    }else{
        float shaped=diffuse*.62+fold*.38;
        lit=color.rgb*(vec3(.46,.42,.38)+vec3(.50,.42,.34)*shaped*visibility);
        float fill=max(dot(N,skyFill),0.0);
        lit+=color.rgb*vec3(.05,.06,.07)*fill;
        float up=max(dot(N,normalize(worldUpEye)),0.0);
        lit+=color.rgb*vec3(.04,.04,.035)*up*shaped;
        float rim=pow(1.0-max(dot(N,V),0.0),2.4);
        lit+=vec3(.11,.09,.07)*rim*(.22+.78*shaped);
    }
    if(materialKind==18||materialKind==39){
        // Warm reflected light on the metal, with a broad sky band and a tight key glint.
        float rim=pow(1.0-clamp(dot(N,V),0.0,1.0),4.0);
        float sky=pow(max(dot(reflect(-V,N),skyFill),0.0),9.0);
        float key=pow(max(dot(R,V),0.0),64.0)*sqrt(diffuse)*visibility;
        float broad=pow(max(dot(R,V),0.0),14.0)*visibility;
        // Pin-point glint on top of the broad band: this is what makes the small
        // hairpins and beads read as polished metal rather than painted gold.
        float hot=pow(max(dot(R,V),0.0),220.0)*visibility;
        vec3 gold=vec3(.78,.55,.25);
        lit=gold*(.34+.50*diffuse*visibility+.20*rim)
            +vec3(.86,.76,.52)*(.34*sky+.20*broad)
            +vec3(1.0,.94,.73)*(key*.92+hot*1.05);
    }else if(drape){
        float back=max(-dot(N,L),0.0);
        // Thin silk has broad diffuse transmission, without a padded specular ridge.
        vec3 layerTint=materialKind==26?vec3(.93,1.0,.98):(materialKind==28?vec3(.92,.96,1.0):vec3(1.0,.98,.92));
        vec3 soft=color.rgb*layerTint*(.48+.32*diffuse*visibility+.19*back);
        lit=mix(lit,soft,.68);
    }else if(airy){
        float back=max(-dot(N,L),0.0);
        float rim=pow(1.0-abs(dot(N,V)),2.0);
        // A light scattering base keeps the thin outer cloth from looking metallic.
        lit=color.rgb*(.71+.23*diffuse+.15*back)+vec3(.05,.055,.048)*rim;
    }else if(materialKind==19){
        float back=max(-dot(N,L),0.0);
        float sheen=pow(max(dot(R,V),0.0),12.0)*diffuse;
        vec2 trim=gauzeTrim(textureCoord);
        vec3 ivory=color.rgb*(.92+.12*diffuse+.10*back)+vec3(.08,.07,.05)*sheen;
        ivory+=color.rgb*vec3(1.0,.94,.84)*pow(back,1.4)*.55;
        float gilding=clamp(.94*trim.x+.31*trim.y,0.0,1.0);
        lit=mix(ivory,vec3(.91,.68,.38)*(1.0+.13*sheen),gilding);
    }
    if(useTexture==1 && (materialKind==0||materialKind==37||materialKind==38)){
        float sleeve=smoothstep(.02,.10,color.g-color.r)*smoothstep(.22,.48,color.g);
        if(ridge>=.2)sleeve=0.0;
        float worldY=dot(eyePosition,worldUpEye)+cameraHeight;
        if(worldY<.85)sleeve=0.0;
        float jadeB=mix(lit.b,lit.g-.03,.8);
        lit=mix(lit,vec3(lit.r,lit.g,min(jadeB,lit.g-.012)),sleeve);
        float green=smoothstep(.05,.14,lit.g-lit.r)*smoothstep(.0,.08,lit.g-lit.b);
        float held=green*(1.0-sleeve)*(1.0-smoothstep(.15,.40,ridge));
        lit=mix(lit,vec3(.38,.40,.34),held*.78);
        float cream=smoothstep(.55,.78,color.r)*smoothstep(.02,.10,color.g-color.b)*smoothstep(.55,.75,color.g/max(color.r,.001));
        lit=mix(lit,vec3(.94,.86,.76),cream*.7);
    }
)"
R"(
    if(materialKind==35){
        lit=color.rgb*(.86+.14*clamp(dot(N,L)*.5+.5,0.0,1.0))*(.96+.04*visibility);
    }
    if(materialKind==33){
        // Darker, more natural stone: cool greenish shadow, warm tan top, no blue lift.
        float top=smoothstep(.48,.92,dot(N,normalize(worldUpEye)));
        vec3 side=color.rgb*vec3(.66,.68,.60);
        vec3 soil=color.rgb*vec3(.72,.66,.52);
        lit=mix(side,soil,top)*(.90+.10*visibility)*(.90+.10*fold);
    }
    if(materialKind==31){
        float up=dot(N,normalize(worldUpEye));
        float top=smoothstep(.62,.96,up);
        vec3 side=mix(vec3(.26,.33,.35),vec3(.39,.45,.44),fold);
        float value=dot(color.rgb,vec3(.25,.50,.25));
        // 顶面压暗偏土黄，避免左岸台地在暮色里泛白
        lit=mix(side,vec3(.50,.44,.35),top)*mix(.72,1.0,smoothstep(.18,.72,value))*(.84+.16*visibility);
    }
    if(materialKind==32){
        bool canopy=textureCoord.x>.375&&textureCoord.x<.75&&textureCoord.y>.25&&textureCoord.y<.375;
        if(canopy){
            float top=smoothstep(.05,.85,dot(N,normalize(worldUpEye)));
            lit=mix(vec3(.53,.40,.27),vec3(.86,.73,.53),.28+.45*top+.27*fold)*(.88+.12*visibility);
        }else{
            lit=mix(vec3(.28,.20,.14),vec3(.61,.46,.30),.30+.70*fold)*(.85+.15*visibility);
        }
    }
    if(materialKind==36){
        // The mass stays black and keeps its weight. Only a few fine strand edges
        // catch the sunset, so the silhouette reads as hair rather than as a shape.
        float thinEdge=pow(1.0-clamp(dot(N,V),0.0,1.0),3.6);
        float strands=noise(modelPos.xz*26.0+vec2(modelPos.y*11.0,modelPos.x*3.0));
        float sparse=smoothstep(.52,.86,strands);
        lit+=vec3(1.0,.84,.60)*thinEdge*(.16+.46*sparse);
    }
    // Preserve the authored rock face roles with an illustration palette.
    if(materialKind==0 && useTexture==0){
        vec3 facePalette=vec3(0.0);
        if(distance(baseColor,vec3(.725,.678,.592))<.003)facePalette=vec3(.76,.70,.60);
        else if(distance(baseColor,vec3(.592,.600,.549))<.003)facePalette=vec3(.58,.61,.60);
        else if(distance(baseColor,vec3(.435,.494,.482))<.003)facePalette=vec3(.40,.49,.53);
        else if(distance(baseColor,vec3(.318,.373,.376))<.003)facePalette=vec3(.28,.35,.39);
        if(facePalette.r>0.0)lit=facePalette*(.82+.18*fold)*(.85+.15*visibility);
    }
    // Exact authored untextured petal palette; exclude stamens, skin and bark.
    if(materialKind==0 && useTexture==0){
        float pale=min(distance(baseColor,vec3(.99,.83,.69)),distance(baseColor,vec3(.99,.87,.72)));
        float rose=distance(baseColor,vec3(.96,.71,.61));
        if(min(pale,rose)<.005){
            vec3 petal=rose<.005?vec3(.94,.76,.75):vec3(.98,.92,.86);
            lit=petal*(vec3(.72,.73,.76)+vec3(.26,.22,.18)*fold*visibility);
        }
    }
    float specStrength=materialKind==1?.006:(materialKind==7?.035:.010);
    vec3 specColor=materialKind==1?vec3(1.0,.94,.82):vec3(1.0,.88,.76);
    if(!silk && materialKind!=33 && materialKind!=31)lit+=specColor*specular*visibility*specStrength;
    // 神光离合: warm and cool fills run in antiphase, so the lit side and the
    // shadow side trade emphasis while the figure keeps a steady overall value.
    // Both lights swing +/-15% of their own base; nothing blooms or outlines.
    if(reflectionPass==0){
        float phase=sin(1.2566370614*spiritTime);
        vec3 toFill=goddessLightEye-eyePosition;
        float distance2=dot(toFill,toFill);
        float reach=1.0-smoothstep(1.0,6.25,distance2);
        if(reach>0.002){
            vec3 fillDir=toFill*inversesqrt(max(distance2,.0001));
            float wrap=.45+.55*max(dot(N,fillDir),0.0);
            // Sunset warmth rides the key-facing side; jade coolness lifts the rest.
            float keySide=clamp(dot(N,L),0.0,1.0);
            float warm=.08*(1.0+.15*phase);
            float cool=.08*(1.0-.15*phase);
            float share=(goddessDraw==1?1.0:.34)*reach;
            vec3 albedo=mix(color.rgb,vec3(dot(color.rgb,vec3(.30,.55,.15))),.30);
            lit+=spiritEnabled*share*albedo*wrap*(vec3(1.0,.95,.86)*warm+vec3(.76,.94,1.0)*cool);
            // The handover redistributes light already on the figure. It must cover
            // her whole body: gating it by distance left the skirt almost static.
            float bodyShare=goddessDraw==1?1.0:reach*.34;
            lit*=1.0+spiritEnabled*bodyShare*.12*phase*(2.0*keySide-1.0);
        }
    }
    float fog=1.0-exp(-max(length(eyePosition)-22.0,0.0)*(materialKind==4?.004:(materialKind==1?.003:.004)));
    float worldHeight=dot(eyePosition,worldUpEye)+cameraHeight;
    // Contact shadow: a soft disc of darker water under her, so she stands in the
    // river instead of floating above it. Fades out well before the reflection.
    if(materialKind==1){
        vec2 contactXZ=textureCoord*10.0-footPosition.xz;
        lit*=mix(.58,1.0,smoothstep(.05,.95,length(contactXZ)));
    }
    if(goddessDraw==1){
        vec3 mp=modelPos;
        // ---- Contact shadows, authored in rest-pose coordinates --------------
        // Each is thin and local; the sum is capped so nothing turns to soot.
        float occl=0.0;
        // chin meeting neck
        float chin=smoothstep(1.335,1.395,mp.y)*(1.0-smoothstep(1.430,1.495,mp.y));
        occl+=.44*chin*(1.0-smoothstep(.050,.135,length(vec2(mp.x-.062,mp.z-.052))));
        // nape and the shaded side of the neck under the hair
        float nape=smoothstep(1.270,1.345,mp.y)*(1.0-smoothstep(1.430,1.520,mp.y));
        occl+=.30*nape*smoothstep(.055,-.055,mp.z);
        // temple and ear, where hair and veil touch the cheek
        float temple=smoothstep(1.415,1.470,mp.y)*(1.0-smoothstep(1.525,1.575,mp.y));
        occl+=.24*temple*smoothstep(.052,.112,abs(mp.x));
        // thin line under the waist sash
        float belt=smoothstep(.950,1.028,mp.y)*(1.0-smoothstep(1.020,1.072,mp.y));
        occl+=.36*belt;
        // sleeve swallowing the wrist
        occl+=.40*(1.0-smoothstep(.050,.140,length(vec3(mp.x-.243,mp.y-.985,mp.z-.357))));
        // pleat hollows and stacked layers
        occl+=.24*valley+.16*overlap;
        if(materialKind==15||materialKind==25)occl+=.12;
        // hem gathering weight at the waterline
        occl+=.26*(1.0-smoothstep(.015,.200,mp.y));
        occl=clamp(occl,0.0,.78);
        // Cool grey, so shadows read as sky-filled shade rather than soot.
        lit*=mix(vec3(.70,.74,.80),vec3(1.0),1.0-occl*.90);
        // Raise the darks and trim contrast so the cloth reads airy, not carved.
        // Gold and the face keep their own authored response.
        if(materialKind!=18 && materialKind!=34 && materialKind!=36 && materialKind!=39){
            float tl=dot(lit,vec3(.299,.587,.114));
            lit+=vec3(.085,.090,.105)*(1.0-smoothstep(.04,.42,tl));
            lit=mix(vec3(tl),lit,.94);
            lit+=color.rgb*.035;
        }
    }
    vec3 fogColor=materialKind==1?vec3(.58,.66,.72):(materialKind==4?vec3(.62,.70,.76):vec3(.78,.70,.62));
    lit=mix(lit,fogColor,clamp(fog,0.0,.02));
    if(ridge>.5){
        vec3 mesh=(gl_ModelViewMatrixInverse*vec4(eyePosition,1.0)).xyz;
        float base=1.0-smoothstep(.0,.46,mesh.y);
        float aerial=smoothstep(28.0,105.0,length(eyePosition));
        lit=mix(lit,mix(vec3(.40,.49,.50),vec3(.70,.71,.67),aerial),base*.32);
    }
    // ---- Low cloud gathered at her feet -------------------------------------
    // A thin, drifting mist layer hugging the waterline around her. The hem of the
    // skirt dissolves into it, which is what makes the figure read as not quite
    // standing on the water. Height falloff keeps her head and shoulders clear.
    if(reflectionPass==0){
        vec3 worldPos=(gl_ModelViewMatrixInverse*vec4(eyePosition,1.0)).xyz;
        vec2 fromHer=worldPos.xz-goddessPosition.xz;
        float dist=length(fromHer);
        float pool=1.0-smoothstep(.25,2.30,dist);
        // A low band hugs the waterline while a taller veil wraps her skirt and waist;
        // the head and shoulders stay clear.
        float shaft=exp(-max(worldPos.y,0.0)*.55)*(1.0-smoothstep(1.75,2.5,worldPos.y));
        float lower=exp(-max(worldPos.y,0.0)*2.2);
        float wisp=noise(worldPos.xz*2.60+vec2(spiritTime*.22,-spiritTime*.14));
        float wisp2=noise(worldPos.xz*6.10-vec2(spiritTime*.31,spiritTime*.09));
        float wisp3=noise(worldPos.xz*1.45+vec2(-spiritTime*.10,spiritTime*.16));
        float body=.40+.60*smoothstep(.30,.90,wisp*.52+wisp2*.34+wisp3*.34);
        float breathe=.84+.16*sin(spiritTime*.40+dist*1.1);
        float density=clamp(pool*(shaft*.78+lower*.30)*body*breathe,0.0,1.0)*.96;
        lit=mix(lit,vec3(.87,.91,.95),density);
    }
    float opacity=materialKind==1?.80:1.0;
    if(reflectionPass==1){
        float depth=max(-worldHeight,0.0);
        lit=mix(lit,vec3(.84,.70,.58),.07);
        opacity=.82*exp(-depth*.42);
    }
    // Water stays on its original plane; wake normals only tint small ripples.
    if(reflectionPass==0 && materialKind!=1){
        float surfaceDistance=length(eyePosition);
        vec3 worldRay=normalize(camRight*eyePosition.x+camUp*eyePosition.y+camBack*eyePosition.z);
        lit=throughValleyCloud(lit,worldRay,surfaceDistance);
    }
    gl_FragColor=vec4(lit,opacity*veilAlpha);
}
)";

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = pCreateShader(type);

    pShaderSource(shader, 1, &source, nullptr);
    pCompileShader(shader);

    GLint success = 0;
    pGetShaderiv(shader, COMPILE_STATUS, &success);

    if (!success)
    {
        char message[4096] = {};
        pGetShaderLog(shader, 4096, nullptr, message);
        std::cerr << "Shader compilation failed:\n"
            << message << "\n";
        pDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createPhongProgram()
{
    GLuint vs = compileShader(
        VERTEX_SHADER, vertexShaderSource);
    if (!vs) return 0;

    GLuint fs = compileShader(
        FRAGMENT_SHADER, fragmentShaderSource);
    if (!fs)
    {
        pDeleteShader(vs);
        return 0;
    }

    GLuint program = pCreateProgram();

    pAttachShader(program, vs);
    pAttachShader(program, fs);
    pLinkProgram(program);

    GLint success = 0;
    pGetProgramiv(program, LINK_STATUS, &success);

    pDeleteShader(vs);
    pDeleteShader(fs);

    if (!success)
    {
        char message[4096] = {};
        pGetProgramLog(program, 4096, nullptr, message);
        std::cerr << "Shader link failed:\n"
            << message << "\n";
        pDeleteProgram(program);
        return 0;
    }

    return program;
}

// ==========================================
// 3. 图片加载与 OpenGL 纹理
// 支持 PNG、JPG、BMP
// ==========================================

bool loadImage(
    const std::string& path,
    std::vector<unsigned char>& pixels,
    unsigned& width,
    unsigned& height)
{
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    bool success = false;

    do
    {
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));

        if (FAILED(hr)) break;

        std::wstring widePath = std::filesystem::u8path(path).wstring();

        hr = factory->CreateDecoderFromFilename(
            widePath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder);

        if (FAILED(hr)) break;

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) break;

        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);

        if (FAILED(hr)) break;

        UINT w = 0, h = 0;
        hr = converter->GetSize(&w, &h);
        if (FAILED(hr) || w == 0 || h == 0) break;

        // 限制尺寸，避免分配过大的缓冲区
        if (w > 8192 || h > 8192) break;

        pixels.resize(
            static_cast<size_t>(w) * h * 4);

        hr = converter->CopyPixels(
            nullptr,
            w * 4,
            static_cast<UINT>(pixels.size()),
            pixels.data());

        if (FAILED(hr)) break;

        width = w;
        height = h;

        // WIC 图像从顶部开始，OpenGL 从底部开始
        const size_t stride = static_cast<size_t>(w) * 4;

        for (unsigned y = 0; y < h / 2; y++)
        {
            size_t top = static_cast<size_t>(y) * stride;
            size_t bottom =
                static_cast<size_t>(h - 1 - y) * stride;

            for (size_t x = 0; x < stride; x++)
                std::swap(pixels[top + x],
                    pixels[bottom + x]);
        }

        success = true;
    } while (false);

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();

    return success;
}

GLuint createTexture(
    const std::string& path,
    bool flag)
{
    std::vector<unsigned char> pixels;
    unsigned width = 0;
    unsigned height = 0;

    if (!loadImage(path, pixels, width, height))
    {
        std::cerr << "ERROR: required texture could not be decoded: " << path << "\n";
        return 0;
    }
    else
    {
        std::cout << "Texture loaded: "
            << path << " (" << width << "x" << height << ")\n";
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, 0x8191, GL_TRUE); // GL_GENERATE_MIPMAP (2.1 compatibility)
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        flag ? 0x812F : GL_REPEAT);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        flag ? 0x812F : GL_REPEAT);

    return texture;
}

// ==========================================
// 4. 原来的 OBJ 加载功能
// ==========================================

