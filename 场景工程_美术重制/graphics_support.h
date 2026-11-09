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
varying vec2 textureCoord;
varying vec4 shadowCoord;
uniform int materialKind;
uniform float sceneTime;
uniform float clothTime;
uniform float clothStrength;
uniform int clothMotionEnabled;
uniform int reflectionPass;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform mat4 shadowFromEye;

void main()
{
    vec4 local=gl_Vertex;
    if(materialKind==3){
        local.x+=.025*sin(local.y*4.0+sceneTime*1.1)*sin(local.z*3.0);
        local.z+=.035*sin(local.y*3.0-sceneTime*.8);
    }
    if(materialKind==5)local.x+=.04*sin(sceneTime*.8+local.z*.9+local.x)*clamp(gl_MultiTexCoord0.y,0.0,1.0);
    if(clothMotionEnabled==1 && materialKind>=8 && materialKind<=16){
        // Ribbon and gold edging share the same displacement field.
        float s;
        vec3 offset;
        if(materialKind==8){
            s=clamp((-local.x-.08)/1.06,0.0,1.0);
            float phase=.86*clothTime-6.15*s;
            float echo=.57*clothTime-3.8*s+.6;
            offset=vec3(.010*sin(phase+.55),.036*sin(phase)+.010*sin(echo),
                        .031*sin(phase+1.24)+.011*sin(echo+1.6));
        }else if(materialKind==9){
            // The loop attaches near the shoulder and returns toward the waist.
            float along=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
            s=sin(3.14159265*along);
            float phase=.63*clothTime-6.0*along;
            offset=vec3(.025*sin(phase-.55),.032*sin(phase),.048*sin(phase+1.38));
        }else if(materialKind==15){
            // Only the lower inner skirt breathes; upper layers stay anchored.
            s=clamp((.60-local.y)/.59,0.0,1.0);
            float phase=.64*clothTime-2.1*s+7.5*local.x+5.4*local.z;
            offset=vec3(.013*sin(phase+.65),.009+.008*sin(phase),.013*sin(phase+1.25));
        }else if(materialKind==14){
            s=clamp((1.055-local.y)/.19,0.0,1.0);
            float phase=.91*clothTime-3.0*s+1.7;
            offset=vec3(.012*sin(phase),.005*sin(phase+.7),.016*sin(phase+1.1));
        }else{
            // The folded shells use V as a shared root-to-tip coordinate on both sides.
            s=clamp((.88-gl_MultiTexCoord0.y)/.75,0.0,1.0);
            if(materialKind==11 || materialKind==12){
                float u=clamp((gl_MultiTexCoord0.x-.18)/.64,0.0,1.0);
                float edge=2.0*u-1.0;
                float freeEdge=smoothstep(.54,1.0,s)*edge*abs(edge);
                if(materialKind==11){
                    // The broad upper panel trails back. Its hem turns across the flow.
                    float phase=.66*clothTime-6.2*s+2.3*u+.45;
                    float roll=freeEdge*sin(.49*clothTime-4.1*s+.8);
                    offset=vec3(.008*sin(phase-.4),
                                .017+.014*sin(phase+.9)+.013*roll,
                                .030*sin(phase)+.012*roll);
                }else{
                    // The lower train sweeps forward, with a delayed, freer hem.
                    float phase=.77*clothTime-6.7*s-1.7*u-1.1;
                    float echo=.53*clothTime-3.9*s+1.4*u;
                    float roll=freeEdge*sin(echo+.8);
                    offset=vec3(-.013*sin(phase-.65)-.006*sin(echo),
                                .025+.014*sin(phase+1.1)+.008*roll,
                                .033*sin(phase)+.011*sin(echo)+.017*roll);
                }
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
        float breeze=(materialKind==15||materialKind==12)?1.0:1.38;
        // Keep the cuffs and tucked seams steady. Amplify only the parts in the wind.
        float freeEnd=smoothstep(.23,.95,s);
        vec3 gain=vec3(1.0);
        if(materialKind==10||materialKind==16)gain=vec3(1.32,1.72,1.56);
        else if(materialKind==11||materialKind==12)gain=vec3(1.28,1.58,1.46);
        else if(materialKind==13)gain=vec3(1.29,1.54,1.46);
        else if(materialKind==8||materialKind==9)gain=vec3(1.34,1.56,1.50);
        local.xyz+=offset*mix(vec3(1.0),gain,freeEnd)*pin*pin*breeze
                    *clothStrength*smoothstep(0.0,.9,clothTime);
    }
    vec4 position = gl_ModelViewMatrix * local;
    if(reflectionPass==1){
        float depth=max(-(dot(position.xyz,worldUpEye)+cameraHeight),0.0);
        position.x+=(.008+.023*depth)*sin(depth*33.0+sceneTime*.9+position.z*.25);
    }

    shadowCoord=shadowFromEye*position;
    eyePosition = position.xyz;
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
uniform vec3 goddessPosition;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform vec3 lightPositionEye;
uniform vec3 baseColor;
uniform int useTexture;
uniform int materialKind;
uniform float sceneTime;
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec2 textureCoord;
varying vec4 shadowCoord;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float shadowTap(vec3 sc,vec2 offset,float bias){return shadow2D(shadowMap,vec3(sc.xy+offset*2.0/2048.0,sc.z-bias)).r;}
// Elliptical reflection from aligned silk threads. Two widths give the
// travelling highlight a soft shoulder without flattening the entire fold.
float silkHighlight(vec3 N,vec3 warp,vec3 weft,vec3 V,vec3 light){
    vec3 halfSum=light+V;
    vec3 H=halfSum*inversesqrt(max(dot(halfSum,halfSum),1e-8));
    float facing=dot(N,H);
    vec2 tilt=vec2(dot(warp,H),dot(weft,H))/max(facing,.25);
    vec2 slope=tilt*tilt;
    float peak=exp(-dot(slope,vec2(5.0,24.0)));
    float shoulder=exp(-dot(slope,vec2(2.4,8.5)));
    return (.72*peak+.28*shoulder)*sqrt(max(dot(N,light),0.0))
           *smoothstep(.08,.38,facing);
}
void main(){
    vec4 color=vec4(baseColor,1.0);
    if(useTexture==1)color*=texture2D(texture0,textureCoord);
    if(color.a<0.44)discard;
    float veilAlpha=1.0;
    bool isVeil=materialKind==9||materialKind==16;
    if(isVeil){
        float u=clamp((textureCoord.x-.18)/.64,0.0,1.0);
        float edge=abs(u*2.0-1.0);
        if(materialKind==9){
            veilAlpha=mix(.68,.36,smoothstep(.13,1.0,edge));
            if(veilProbe==1)veilAlpha=.14;
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
    if(materialKind>=8 && materialKind<=16){
        // Geometric normals follow the deformed triangle without smoothing the folds.
        vec3 facet=cross(dFdx(eyePosition),dFdy(eyePosition));
        if(dot(facet,facet)>1e-14){
            facet=normalize(facet);
            facet=dot(facet,N)<0.0?-facet:facet;
            // Curved outer cloth retains its loft normals; the facet term follows motion.
            N=materialKind==14?facet:normalize(mix(N,facet,materialKind==15?.27:.14));
        }
    }
    if(materialKind==1){
        float wave=sin(textureCoord.x*9.0+sceneTime*.65)+sin(textureCoord.y*17.0+textureCoord.x*3.0-sceneTime*.43);
        N=normalize(N+vec3(.035*wave,0,.055*cos(textureCoord.y*12.0+sceneTime*.5)));
        color.rgb=mix(vec3(.075,.235,.275),vec3(.16,.36,.39),.5+.045*wave);
        float lines=pow(.5+.5*sin(textureCoord.y*39.0+sin(textureCoord.x*8.0+sceneTime*.2)*3.0),18.0);
        color.rgb+=lines*.010;
        vec2 waterWorld=textureCoord*10.0;
        float band=exp(-pow((waterWorld.x-.15*waterWorld.y)/(.14+.035*abs(waterWorld.y)),2.0));
        float glimmer=pow(.5+.5*sin(waterWorld.y*31.0+sin(waterWorld.x*17.0+sceneTime*.5)*2.0-sceneTime*.8),16.0)*pow(.5+.5*sin(waterWorld.x*83.0+waterWorld.y*47.0+sceneTime*.6),9.0);
        color.rgb+=vec3(.24,.16,.07)*band*glimmer;
        float grazing=pow(1.0-max(dot(N,normalize(-eyePosition)),0.0),3.0);
        color.rgb=mix(color.rgb,vec3(.53,.64,.62),grazing*.60);
        vec2 rippleOffset=textureCoord*10.0-goddessPosition.xz;
        float angle=atan(rippleOffset.y,rippleOffset.x);
        float radius=length(rippleOffset*vec2(.94,1.08))+.035*sin(angle*3.0+sceneTime*.2);
        float arcs=smoothstep(.20,.65,.5+.5*sin(angle*3.0+radius*2.0-sceneTime*.25));
        color.rgb+=vec3(.035,.046,.04)*pow(.5+.5*cos(radius*18.0-sceneTime*1.5),14.0)*exp(-radius*1.9)*arcs;
    }
    if(materialKind==2){
        float patch=.5+.5*sin(textureCoord.x*2.0+sin(textureCoord.y*1.8))*cos(textureCoord.y*2.4);
        color.rgb*=mix(vec3(.80,.84,.72),vec3(1.04,.99,.88),patch);
        color.rgb*=.97+.04*hash(floor(textureCoord*90.0));
        vec2 ground=textureCoord/0.4;
        color.rgb=mix(color.rgb,vec3(.19,.28,.18),.60);
        float shore=-3.5+.65*sin(ground.y*.16)+.35*sin(ground.y*.53+.8)+.12*sin(ground.y*1.4)-ground.x;
        color.rgb=mix(color.rgb,vec3(.25,.26,.20),.4*exp(-max(shore,0.0)*2.0));
        float shade=exp(-dot((ground-vec2(-8.0,-3.5))/vec2(2.0,2.7),(ground-vec2(-8.0,-3.5))/vec2(2.0,2.7)));
        color.rgb*=1.0-.22*shade;
        vec2 contact=(ground-vec2(-4.3,1.0))/vec2(.55,.4);
        color.rgb*=1.0-.38*exp(-dot(contact,contact));
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
    vec3 L=normalize(lightPositionEye-eyePosition),V=normalize(-eyePosition),R=reflect(-L,N);
    float diffuse=max(dot(N,L),0.0);
    float specular=diffuse>0.0?pow(max(dot(R,V),0.0),materialKind==1?105.0:(materialKind==7?70.0:35.0)):0.0;
    float visibility=1.0;
    vec3 sc=shadowCoord.xyz/shadowCoord.w;
    if(shadowsEnabled==1 && reflectionPass==0 && materialKind!=1 && materialKind!=4 && sc.x>0.0 && sc.x<1.0 && sc.y>0.0 && sc.y<1.0 && sc.z>0.0 && sc.z<1.0){
        float bias=max(.0008,.0018*(1.0-max(dot(N,L),0.0)));
        // The thin double-sided cloth shares almost the same shadow depth.
        if(materialKind==10||materialKind==11||materialKind==12||materialKind==13)bias+=.00045;float sum=0.0;
        // Four hardware-filtered depth comparisons soften the edge with fewer fetches.
        sum+=shadowTap(sc,vec2(.35,.45),bias);
        sum+=shadowTap(sc,vec2(-.45,.25),bias);
        sum+=shadowTap(sc,vec2(.25,-.35),bias);
        sum+=shadowTap(sc,vec2(-.35,-.45),bias);
        visibility=.18+.82*sum/4.0;
    }
    bool ivory=materialKind==11||materialKind==12||materialKind==15;
    bool sleeve=materialKind==10||materialKind==13||materialKind==17;
    bool silk=materialKind>=8 && materialKind<=17;
    vec3 directTint=vec3(.88,.70,.49);
    if(ivory)directTint=vec3(.63,.59,.52);
    else if(sleeve)directTint=vec3(.69,.60,.48);
    else if(materialKind==9)directTint=vec3(.67,.56,.40);
    vec3 skyFill=normalize(vec3(.6,.4,1.0));
    vec3 lit=color.rgb*(vec3(.31,.36,.39)+directTint*diffuse*visibility);
    lit+=color.rgb*vec3(.12,.19,.23)*max(dot(N,skyFill),0.0);
    if(silk){
        // Reconstruct the fabric direction after deformation. Use world up
        // projected onto the cloth where an atlas seam has degenerate UVs.
        vec3 warp=dFdy(eyePosition)*dFdx(textureCoord.x)
                 -dFdx(eyePosition)*dFdy(textureCoord.x);
        warp-=N*dot(N,warp);
        if(dot(warp,warp)<1e-16)warp=worldUpEye-N*dot(N,worldUpEye);
        if(dot(warp,warp)<1e-16){
            vec3 axis=abs(N.z)<.90?vec3(0.0,0.0,1.0):vec3(1.0,0.0,0.0);
            warp=cross(N,axis);
        }
        warp=normalize(warp);
        vec3 weft=cross(N,warp);
        vec2 strength=ivory?vec2(.43,.24):(sleeve?vec2(.38,.23):vec2(.22,.09));
        if(materialKind==15)strength*=.79;
        else if(materialKind==17)strength=vec2(.25,.13);
        else if(isVeil)strength=vec2(.12,.05);
        vec3 tint=mix(color.rgb,ivory?vec3(1.0,.96,.85):vec3(.89,.96,.93),.63);
        float keySilk=silkHighlight(N,warp,weft,V,L);
        float skySilk=silkHighlight(N,warp,weft,V,skyFill);
        vec3 glint=tint*(strength.x*keySilk*visibility+strength.y*skySilk);
        // Roll off the crest as it approaches display white, preserving
        // the color and separation of adjacent folds in side views.
        float headroom=max(1.0-max(lit.r,max(lit.g,lit.b)),0.0);
        float glintPeak=max(glint.r,max(glint.g,glint.b));
        lit+=glint*headroom/(headroom+glintPeak+1e-5);
        if(isVeil)lit+=color.rgb*.037*max(-dot(N,L),0.0)*visibility;
    }else{
        lit+=vec3(1.0,.85,.65)*specular*visibility
            *((materialKind==1||materialKind==7)?.18:.035);
    }
    // Broad cool ridge tones keep distant scenery subordinate to the figures.
    if(materialKind==4)lit=color.rgb*(.85+.15*max(dot(N,normalize(vec3(-.6,1.0,.3))),0.0));
    float fog=1.0-exp(-max(length(eyePosition)-8.0,0.0)*(materialKind==4?.019:.022));
    float worldHeight=dot(eyePosition,worldUpEye)+cameraHeight;
    float mist=.20*(1.0-exp(-length(eyePosition)*.045))*exp(-max(worldHeight,0.0)*.8);
    fog=1.0-(1.0-fog)*(1.0-mist);
    vec3 fogColor=materialKind==1?vec3(.57,.67,.64):vec3(.74,.72,.65);
    lit=mix(lit,fogColor,clamp(fog,0.0,.86));
    float opacity=materialKind==1?.86:1.0;
    if(reflectionPass==1){
        float depth=max(-worldHeight,0.0);
        lit=mix(lit,vec3(.38,.48,.46),.50);
        opacity=.72*exp(-depth*.65);
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

