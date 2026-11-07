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
uniform vec3 goddessPosition;
uniform vec3 worldUpEye;
uniform float cameraHeight;
uniform vec3 lightPositionEye;
uniform vec3 baseColor;
uniform int useTexture;
uniform int materialKind;
uniform float sceneTime;
uniform vec3 cloudParams;
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
void main(){
    if(materialKind==8 && renderDepth==0){
        vec2 uv=gl_FragCoord.xy/max(cloudParams.yz,vec2(1.0));
        vec2 ndc=uv*2.0-1.0;
        float aspect=max(cloudParams.y/max(cloudParams.z,1.0),.5);
        vec3 rd=normalize(camRight*(ndc.x*aspect*.4142)+camUp*(ndc.y*.4142)-camBack);
        vec3 sunDir=normalize(vec3(-.15,.05,-1.0));
        float ang=acos(clamp(dot(rd,sunDir),-1.0,1.0));
        float sunClear=smoothstep(.05,.18,ang);
        float drift=sceneTime*.012;
        float wx=rd.x*rd.x, wz=rd.z*rd.z, wsum=max(wx+wz,1e-3);
        vec2 scale=vec2(2.4,2.4);
        float nx=noise(rd.yz*scale+vec2(drift,1.3));
        float nz=noise(rd.xy*scale+vec2(4.2,drift));
        float n=(nx*wx+nz*wz)/wsum;
        float puff=smoothstep(.46,.62,n);
        puff*=smoothstep(.08,.2,rd.y)*(1.0-smoothstep(.62,.9,rd.y))*sunClear;
        float skyBand=exp(-pow((rd.y-.05)/.04,2.0))*smoothstep(.01,.05,rd.y)*sunClear;
        float upper=smoothstep(.02,.2,rd.y);
        vec3 apricot=mix(vec3(.95,.78,.62),vec3(.97,.83,.70),smoothstep(0.0,.4,rd.y));
        float warmA=upper*.88*sunClear;
        vec3 cloudCol=mix(vec3(.98,.92,.84),vec3(1.0,.97,.94),clamp(n,0.0,1.0));
        vec3 rgb=mix(apricot,cloudCol,clamp(puff,0.0,1.0));
        float alpha=max(warmA,clamp(puff*.5+skyBand*.16,0.0,.55));
        gl_FragColor=vec4(rgb,alpha);
        return;
    }
    vec4 color=vec4(baseColor,1.0);
    if(useTexture==1)color*=texture2D(texture0,textureCoord);
    if(color.a<0.44)discard;
    if(renderDepth==1){gl_FragColor=vec4(1.0);return;}
    if(materialKind==6){gl_FragColor=vec4(.035,.065,.06,.24);return;}
    vec3 N=normalize(eyeNormal);if(!gl_FrontFacing)N=-N;
    if(materialKind==1){
        float wave=sin(textureCoord.x*9.0+sceneTime*.65)+sin(textureCoord.y*17.0+textureCoord.x*3.0-sceneTime*.43);
        N=normalize(N+vec3(.035*wave,0,.055*cos(textureCoord.y*12.0+sceneTime*.5)));
        color.rgb=mix(vec3(.18,.32,.38),vec3(.40,.54,.60),.48+.04*wave);
        float lines=pow(.5+.5*sin(textureCoord.y*39.0+sin(textureCoord.x*8.0+sceneTime*.2)*3.0),18.0);
        color.rgb+=vec3(.55,.62,.66)*lines*.012;
        vec2 waterWorld=textureCoord*10.0;
        float band=exp(-pow((waterWorld.x-.15*waterWorld.y)/(.07+.02*abs(waterWorld.y)),2.0));
        float glimmer=pow(.5+.5*sin(waterWorld.y*31.0+sin(waterWorld.x*17.0+sceneTime*.5)*2.0-sceneTime*.8),16.0)*pow(.5+.5*sin(waterWorld.x*83.0+waterWorld.y*47.0+sceneTime*.6),9.0);
        color.rgb+=vec3(.18,.11,.06)*band*glimmer;
        float grazing=pow(1.0-max(dot(N,normalize(-eyePosition)),0.0),3.0);
        color.rgb=mix(color.rgb,vec3(.52,.62,.68),grazing*.32);
        vec2 rippleOffset=textureCoord*10.0-goddessPosition.xz;
        float angle=atan(rippleOffset.y,rippleOffset.x);
        float radius=length(rippleOffset*vec2(.94,1.08))+.035*sin(angle*3.0+sceneTime*.2);
        float arcs=smoothstep(.20,.65,.5+.5*sin(angle*3.0+radius*2.0-sceneTime*.25));
        color.rgb+=vec3(.05,.07,.08)*pow(.5+.5*cos(radius*18.0-sceneTime*1.5),14.0)*exp(-radius*1.9)*arcs;
    }
    if(materialKind==2){
        float patch=.5+.5*sin(textureCoord.x*2.0+sin(textureCoord.y*1.8))*cos(textureCoord.y*2.4);
        color.rgb*=mix(vec3(.80,.84,.72),vec3(1.04,.99,.88),patch);
        color.rgb*=.97+.04*hash(floor(textureCoord*90.0));
        vec2 ground=textureCoord/0.4;
        color.rgb=mix(color.rgb,vec3(.26,.32,.28),.42);
        float shore=-3.5+.65*sin(ground.y*.16)+.35*sin(ground.y*.53+.8)+.12*sin(ground.y*1.4)-ground.x;
        color.rgb=mix(color.rgb,vec3(.30,.33,.30),.30*exp(-max(shore,0.0)*2.0));
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
    // main.cpp still places the sun at world (-12, 16, 9). Slide it down to y=5.5
    // along world up so the direction stays fixed when the camera moves.
    vec3 sunEye=lightPositionEye-10.5*worldUpEye;
    vec3 L=normalize(sunEye-eyePosition),V=normalize(-eyePosition),R=reflect(-L,N);
    float diffuse=max(dot(N,L),0.0);
    float shininess=materialKind==1?150.0:(materialKind==7?40.0:22.0);
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
        lit=color.rgb*(vec3(.64,.72,.76)+vec3(.08,.12,.14)*diffuse);
    }else if(materialKind==4){
        lit=color.rgb*(vec3(.60,.66,.74)+vec3(.10,.11,.12)*diffuse);
    }else{
        float shaped=diffuse*.75+fold*.25;
        lit=color.rgb*(vec3(.26,.20,.18)+vec3(.68,.40,.18)*shaped*visibility);
        float fill=max(dot(N,normalize(vec3(-.2,.72,.48))),0.0);
        lit+=color.rgb*vec3(.04,.05,.07)*fill;
        float up=max(dot(N,normalize(worldUpEye)),0.0);
        lit+=color.rgb*vec3(.08,.045,.02)*up*shaped;
        float rim=pow(1.0-max(dot(N,V),0.0),2.6);
        lit+=vec3(.10,.055,.03)*rim*(.2+.8*shaped);
    }
    float specStrength=materialKind==1?.03:(materialKind==7?.035:.010);
    vec3 specColor=materialKind==1?vec3(1.0,.94,.82):vec3(1.0,.88,.76);
    lit+=specColor*specular*visibility*specStrength;
    float fog=1.0-exp(-max(length(eyePosition)-10.0,0.0)*(materialKind==4?.008:(materialKind==1?.005:.007)));
    float worldHeight=dot(eyePosition,worldUpEye)+cameraHeight;
    float mist=(materialKind==4?0.0:.04)*(1.0-exp(-length(eyePosition)*.028))*exp(-max(worldHeight,0.0)*1.15);
    fog=1.0-(1.0-fog)*(1.0-mist);
    vec3 fogColor=materialKind==1?vec3(.60,.70,.76):(materialKind==4?vec3(.46,.54,.60):vec3(.82,.78,.74));
    lit=mix(lit,fogColor,clamp(fog,0.0,materialKind==4?.16:.24));
    if(materialKind==4){
        float foot=exp(-pow((worldHeight-1.3)/1.7,2.0));
        float side=.7+.3*noise(textureCoord*vec2(1.4,.7));
        lit=mix(lit,vec3(.66,.75,.82),clamp(foot*side,0.0,.5));
    }
    float opacity=materialKind==1?.86:1.0;
    if(reflectionPass==1){
        float depth=max(-worldHeight,0.0);
        lit=mix(lit,vec3(.84,.70,.58),.07);
        opacity=.68*exp(-depth*.65);
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

