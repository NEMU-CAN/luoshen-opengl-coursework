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
uniform vec3 cloudParams;
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
    if(materialKind==1){
        float anim=sceneTime>0.0?sceneTime:cloudParams.x;
        local.y+=.03*sin(local.x*.05+anim*.35)+.02*sin(local.z*.04-anim*.22);
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
uniform int shadowsEnabled;
uniform int reflectionPass;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform vec3 lightPositionEye;
uniform vec3 baseColor;
uniform int useTexture;
uniform int materialKind;
uniform float sceneTime;
uniform vec3 cloudParams;
uniform vec3 goddessPosition;
uniform vec3 footPosition;
uniform vec3 boatPosition;
uniform vec3 goddessVelocity;
uniform vec3 boatVelocity;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 camBack;
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec2 textureCoord;
varying vec4 shadowCoord;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1.0,0.0)),f.x),mix(hash(i+vec2(0.0,1.0)),hash(i+vec2(1.0,1.0)),f.x),f.y);}
float shadowTap(vec3 sc,vec2 offset,float bias){return shadow2D(shadowMap,vec3(sc.xy+offset*8.0/2048.0,sc.z-bias)).r;}
float wakeHeight(vec2 p,vec2 center,vec2 vel){
    float speed=length(vel);
    float moving=smoothstep(.045,.16,speed);
    vec2 dir=vel/max(speed,.001);
    vec2 rel=p-center;
    float along=dot(rel,dir);
    float side=rel.x*dir.y-rel.y*dir.x;
    float press=exp(-dot(rel,rel)/.85);
    vec2 tail=rel+dir*.65;
    float rim=exp(-pow(length(tail)-1.15,2.0)/.07);
    float rear=smoothstep(.35,-.15,along);
    return (-.28*press+.20*rim*rear)*moving;
}
float wakeAt(vec2 p){
    return wakeHeight(p,footPosition.xz,goddessVelocity.xz)+wakeHeight(p,boatPosition.xz,boatVelocity.xz);
}
void main(){
    gl_FragDepth=gl_FragCoord.z;
    if(materialKind==8 && renderDepth==0){
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
        sky=mix(sky,vec3(.99,.90,.80),disc*.82);
        gl_FragColor=vec4(sky,1.0);
        return;
    }
    vec4 color=vec4(baseColor,1.0);
    if(useTexture==1)color*=texture2D(texture0,textureCoord);
    if(color.a<0.44)discard;
    if(renderDepth==1){gl_FragColor=vec4(1.0);return;}
    if(materialKind==6){gl_FragColor=vec4(.035,.065,.06,.24);return;}
    vec3 N=normalize(eyeNormal);if(!gl_FrontFacing)N=-N;
    float sunBand=0.0;
    vec3 wakePoint=vec3(0.0);
    float wakeMoved=0.0;
    float wakeRaised=0.0;
    vec3 wakeNormal=vec3(0.0,1.0,0.0);
    if(materialKind==1){
        vec2 flow=textureCoord*10.0;
        float away=smoothstep(2.0,-10.0,flow.y);
        color.rgb=mix(vec3(.40,.50,.64),vec3(.48,.58,.70),away);
        vec2 fromSun=flow-vec2(-1.1,-2.4);
        sunBand=exp(-fromSun.x*fromSun.x/(1.15*1.15)-fromSun.y*fromSun.y/(.85*.85));
        if(reflectionPass==0){
            float h=wakeAt(flow);
            if(abs(h)>.003){
                wakeMoved=1.0;
                wakeRaised=step(0.0,h);
                float hx=(wakeAt(flow+vec2(.12,0.0))-wakeAt(flow-vec2(.12,0.0)))/.24;
                float hz=(wakeAt(flow+vec2(0.0,.12))-wakeAt(flow-vec2(0.0,.12)))/.24;
                wakeNormal=normalize(vec3(-hx,1.0,-hz));
                wakePoint=eyePosition+worldUpEye*h;
            }
        }
    }
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
    if(materialKind==0 && useTexture==0){
        float peak=max(color.r,color.g);
        float lift=smoothstep(.22,.30,peak)*(1.0-smoothstep(.42,.70,peak));
        color.rgb=mix(color.rgb,color.rgb*.5+vec3(.46,.44,.39),lift);
    }
    float ridge=0.0;
    if(useTexture==1 && materialKind==0){
        float cool=smoothstep(.0,.07,min(color.g,color.b)-color.r);
        float gray=1.0-smoothstep(.05,.16,abs(color.g-color.b));
        ridge=cool*gray*step(20.0,length(eyePosition));
        color.rgb=mix(color.rgb,color.rgb*vec3(.94,.86,1.06),ridge*.45);
    }
    // main.cpp still places the sun at world (-12, 16, 9). Slide it down to y=5.5
    // along world up so the direction stays fixed when the camera moves.
    vec3 sunEye=lightPositionEye-10.5*worldUpEye;
    vec3 L=normalize(sunEye-eyePosition),V=normalize(-eyePosition),R=reflect(-L,N);
    float diffuse=max(dot(N,L),0.0);
    float shininess=materialKind==1?28.0:(materialKind==7?40.0:22.0);
    float specular=diffuse>0.0?pow(max(dot(R,V),0.0),shininess):0.0;
    float visibility=1.0;
    vec3 sc=shadowCoord.xyz/shadowCoord.w;
    if(shadowsEnabled==1 && reflectionPass==0 && materialKind!=1 && materialKind!=4 && sc.x>0.0 && sc.x<1.0 && sc.y>0.0 && sc.y<1.0 && sc.z>0.0 && sc.z<1.0){
        float bias=max(.0022,.0052*(1.0-diffuse));float sum=0.0;
        // About eight texels wide, so folded-rock edges stay soft and faces are not speckled.
        sum+=shadowTap(sc,vec2(1.05,.72),bias);
        sum+=shadowTap(sc,vec2(-1.12,.48),bias);
        sum+=shadowTap(sc,vec2(.58,-1.02),bias);
        sum+=shadowTap(sc,vec2(-.72,-.95),bias);
        visibility=.55+.45*sum/4.0;
    }
    // Smoothstep keeps low-poly planes apart without driving a back face to black.
    float fold=smoothstep(.08,.82,diffuse);
    vec3 lit;
    if(materialKind==1){
        lit=color.rgb;
        lit=mix(lit,vec3(.78,.58,.46),sunBand*.28);
        if(wakeMoved>.5){
            vec3 nEye=normalize((gl_ModelViewMatrix*vec4(wakeNormal,0.0)).xyz);
            float flatD=max(dot(normalize(worldUpEye),L),0.0);
            float wakeD=max(dot(nEye,L),0.0);
            lit*=1.0+clamp((wakeD-flatD)*1.5,-.16,.12);
        }
    }else if(ridge>.45){
        float shaped=diffuse*.5+fold*.5;
        lit=color.rgb*(vec3(.66,.68,.78)+vec3(.22,.16,.14)*shaped);
        float shade=1.0-smoothstep(.12,.65,shaped);
        lit=mix(lit,vec3(.36,.44,.54),shade*.30);
    }else if(materialKind==4){
        float shaped=clamp(diffuse*.85+.1,0.0,1.0);
        shaped=shaped*.7+smoothstep(.05,.8,diffuse)*.3;
        lit=color.rgb*(vec3(.34,.38,.42)+vec3(.74,.60,.46)*shaped);
    }else if(materialKind==2){
        float shaped=diffuse*.7+fold*.3;
        lit=color.rgb*(vec3(.50,.48,.46)+vec3(.46,.44,.42)*shaped*visibility);
    }else if(materialKind==0 && useTexture==0){
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
        float fill=max(dot(N,normalize(vec3(-.25,.7,.42))),0.0);
        lit+=color.rgb*vec3(.05,.06,.07)*fill;
        float up=max(dot(N,normalize(worldUpEye)),0.0);
        lit+=color.rgb*vec3(.04,.04,.035)*up*shaped;
        float rim=pow(1.0-max(dot(N,V),0.0),2.4);
        lit+=vec3(.11,.09,.07)*rim*(.22+.78*shaped);
    }
    if(useTexture==1 && materialKind==0){
        float sleeve=smoothstep(.02,.10,color.g-color.r)*smoothstep(.22,.48,color.g);
        if(ridge>=.2)sleeve=0.0;
        float worldY=dot(eyePosition,worldUpEye)+cameraHeight;
        if(worldY<.85)sleeve=0.0;
        lit=mix(lit,vec3(lit.r*.82,lit.g*.74,min(lit.b*1.16,lit.g)),sleeve*.8);
        float green=smoothstep(.05,.14,lit.g-lit.r)*smoothstep(.0,.08,lit.g-lit.b);
        float held=green*(1.0-sleeve);
        lit=mix(lit,vec3(.38,.40,.34),held*.78);
        float cream=smoothstep(.55,.78,color.r)*smoothstep(.02,.10,color.g-color.b)*smoothstep(.55,.75,color.g/max(color.r,.001));
        lit=mix(lit,vec3(.94,.86,.76),cream*.7);
    }
    float specStrength=materialKind==1?.006:(materialKind==7?.035:.010);
    vec3 specColor=materialKind==1?vec3(1.0,.94,.82):vec3(1.0,.88,.76);
    lit+=specColor*specular*visibility*specStrength;
    float fog=1.0-exp(-max(length(eyePosition)-22.0,0.0)*(materialKind==4?.004:(materialKind==1?.003:.004)));
    float worldHeight=dot(eyePosition,worldUpEye)+cameraHeight;
    vec3 fogColor=materialKind==1?vec3(.58,.66,.72):(materialKind==4?vec3(.62,.70,.76):vec3(.78,.70,.62));
    lit=mix(lit,fogColor,clamp(fog,0.0,.02));
    if(ridge>.45){
        vec3 mesh=(gl_ModelViewMatrixInverse*vec4(eyePosition,1.0)).xyz;
        vec3 mx=gl_ModelViewMatrix[0].xyz;
        vec3 my=gl_ModelViewMatrix[1].xyz;
        float xScale=length(camRight*mx.x+camUp*mx.y+camBack*mx.z);
        float yScale=length(camRight*my.x+camUp*my.y+camBack*my.z);
        float peakX=1.3;
        if(yScale>3.1)peakX=.8;
        else if(xScale>2.65&&yScale<2.4)peakX=.8;
        else if(xScale>2.55)peakX=3.5;
        float away=smoothstep(1.2,5.5,abs(mesh.x-peakX));
        float rel=clamp(mesh.y/3.2,0.0,1.0);
        float foot=pow(clamp(1.0-smoothstep(.08,.40,rel),0.0,1.0),1.15);
        float notch=pow(clamp(1.0-smoothstep(.22,.80,rel),0.0,1.0),1.1);
        float settle=mix(foot,notch,smoothstep(.2,.8,away));
        float far=smoothstep(22.0,40.0,length(eyePosition));
        lit=mix(lit,vec3(.70,.77,.79),clamp(far*settle,0.0,.70));
    }
    float opacity=materialKind==1?.82:1.0;
    if(reflectionPass==1){
        float depth=max(-worldHeight,0.0);
        lit=mix(lit,vec3(.84,.70,.58),.07);
        opacity=.68*exp(-depth*.65);
    }
    if(materialKind==1 && wakeMoved>.5){
        vec4 clip=gl_ProjectionMatrix*vec4(wakePoint,1.0);
        float displaced=clamp(clip.z/clip.w*.5+.5,0.0,1.0);
        gl_FragDepth=wakeRaised>.5?min(displaced,gl_FragCoord.z):max(displaced,gl_FragCoord.z);
    }
    gl_FragColor=vec4(lit,opacity);
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

