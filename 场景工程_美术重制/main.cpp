// 洛水别绪：曹植与洛神的河岸离别，OBJ贴图、Phong、倒影与Bézier动画。
#include "graphics_support.h"
#include "mesh_data.h"
#include "scene_motion.h"
#include <map>
#include <chrono>
#include <cstddef>
#include <cstring>
#include "scene_overlay.h"
#include "shadow_map.h"
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")

struct GpuBatch {GLuint buffer=0,texture=0;GLsizei count=0;glm::vec3 color{1};int kind=0;};
using GenBuffersFn=void(APIENTRY*)(GLsizei,GLuint*);
using BindBufferFn=void(APIENTRY*)(GLenum,GLuint);
using BufferDataFn=void(APIENTRY*)(GLenum,std::ptrdiff_t,const void*,GLenum);
using DeleteBuffersFn=void(APIENTRY*)(GLsizei,const GLuint*);
GenBuffersFn sceneGenBuffers=nullptr;
BindBufferFn sceneBindBuffer=nullptr;
BufferDataFn sceneBufferData=nullptr;
DeleteBuffersFn sceneDeleteBuffers=nullptr;
const GLenum SCENE_ARRAY_BUFFER=0x8892,SCENE_STATIC_DRAW=0x88E4;
struct SceneModel {std::vector<GpuBatch> batches;glm::vec3 center{0};float bottom=0;size_t triangles=0;};
GLuint program=0;
GLint colorLocation=-1,textureLocation=-1,kindLocation=-1,timeLocation=-1,lightLocation=-1;
GLint reflectionLocation=-1,goddessLocation=-1,worldUpLocation=-1,cameraHeightLocation=-1;
using ActiveTextureFn=void(APIENTRY*)(GLenum);
ActiveTextureFn sceneActiveTexture=nullptr;
bool reflectionPass=false,shadowPass=false,showHelp=false;
glm::mat4 worldPrefix(1);
GLuint groundColor=0,groundNormal=0;
std::map<std::string,GLuint> textures;
std::filesystem::path assetRoot;
bool paused=false,markers=false,captureRequested=false;
double animationTime=0;
float yaw=8.0f,pitch=7.0f,distanceToTarget=11.0f;
glm::vec3 target(-.8f,1.45f,-.6f);
bool leftDown=false,rightDown=false;double lastX=0,lastY=0;

GLuint loadMaterialTexture(const SceneMaterial& mat) {
    if(mat.colorMap.empty())return 0;
    std::string key=mat.colorMap.u8string()+"|"+mat.alphaMap.u8string();
    if(textures.count(key))return textures.at(key);
    std::vector<unsigned char> pixels;unsigned w=0,h=0;
    if(!loadImage(mat.colorMap.u8string(),pixels,w,h))throw std::runtime_error("Cannot decode "+mat.colorMap.u8string());
    if(!mat.alphaMap.empty()){
        std::vector<unsigned char> alpha;unsigned aw=0,ah=0;
        if(!loadImage(mat.alphaMap.u8string(),alpha,aw,ah))throw std::runtime_error("Cannot decode opacity image");
        bool hasAuthoredAlpha=false;
        for(size_t i=3;i<alpha.size();i+=4)if(alpha[i]!=255){hasAuthoredAlpha=true;break;}
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            unsigned ax=std::min(aw-1,x*aw/w),ay=std::min(ah-1,y*ah/h);
            pixels[(size_t(y)*w+x)*4+3]=alpha[(size_t(ay)*aw+ax)*4+(hasAuthoredAlpha?3:0)];
        }
        std::cout<<"Opacity loaded ("<<(hasAuthoredAlpha?"RGBA alpha":"grayscale")<<"): "<<mat.alphaMap.u8string()<<'\n';
    }
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,0x8191,GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    textures[key]=texture;
    std::cout<<"Texture loaded: "<<mat.colorMap.u8string()<<" ("<<w<<"x"<<h<<")\n";
    return texture;
}

SceneModel loadModel(const std::string& relative){
    auto mesh=readSceneObj(assetRoot/std::filesystem::u8path(relative));
    SceneModel model;model.center=(mesh.minimum+mesh.maximum)*.5f;model.bottom=mesh.minimum.y;model.triangles=mesh.triangles;
    for(const auto& batch:mesh.batches){
        const auto& mat=mesh.materials.at(batch.material);
        GpuBatch gpu;gpu.texture=loadMaterialTexture(mat);gpu.color=gpu.texture?glm::vec3(1):mat.diffuse;
        gpu.kind=mat.name.find("Under_robe")!=std::string::npos||mat.name.find("Under robe")!=std::string::npos?3:0;
        if(mat.name.find("Hair_knot")!=std::string::npos)gpu.kind=7;
        sceneGenBuffers(1,&gpu.buffer);if(!gpu.buffer)throw std::runtime_error("Geometry buffer allocation failed");
        gpu.count=static_cast<GLsizei>(batch.vertices.size());
        sceneBindBuffer(SCENE_ARRAY_BUFFER,gpu.buffer);
        sceneBufferData(SCENE_ARRAY_BUFFER,std::ptrdiff_t(batch.vertices.size()*sizeof(SceneVertex)),batch.vertices.data(),SCENE_STATIC_DRAW);
        sceneBindBuffer(SCENE_ARRAY_BUFFER,0);model.batches.push_back(gpu);
    }
    std::cout<<"Model loaded: "<<relative<<" triangles="<<mesh.triangles<<" material_batches="<<model.batches.size()<<'\n';
    return model;
}

void material(glm::vec3 color,int kind=0,GLuint texture=0){
    pUniform3f(colorLocation,color.x,color.y,color.z);pUniform1i(kindLocation,shadowPass?6:kind);
    pUniform1i(reflectionLocation,reflectionPass?1:0);
    pUniform1i(textureLocation,texture?1:0);glBindTexture(GL_TEXTURE_2D,texture);
}
void drawModel(const SceneModel& model,const glm::mat4& view,glm::vec3 position,float angle,float scale,glm::vec3 tint=glm::vec3(1)){
    auto m=glm::translate(worldPrefix*(reflectionPass?glm::scale(glm::mat4(1),glm::vec3(1,-1,1)):glm::mat4(1)),position);
    m=glm::rotate(m,glm::radians(angle),glm::vec3(0,1,0));m=glm::scale(m,glm::vec3(scale));
    m=glm::translate(m,glm::vec3(-model.center.x,-model.bottom,-model.center.z));
    auto mv=view*m;glMatrixMode(GL_MODELVIEW);glLoadMatrixf(glm::value_ptr(mv));
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for(const auto& b:model.batches){
        material(b.color*tint,b.kind,b.texture);sceneBindBuffer(SCENE_ARRAY_BUFFER,b.buffer);
        glVertexPointer(3,GL_FLOAT,sizeof(SceneVertex),reinterpret_cast<const void*>(offsetof(SceneVertex,position)));
        glNormalPointer(GL_FLOAT,sizeof(SceneVertex),reinterpret_cast<const void*>(offsetof(SceneVertex,normal)));
        glTexCoordPointer(2,GL_FLOAT,sizeof(SceneVertex),reinterpret_cast<const void*>(offsetof(SceneVertex,uv)));
        glDrawArrays(GL_TRIANGLES,0,b.count);
    }
    sceneBindBuffer(SCENE_ARRAY_BUFFER,0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_VERTEX_ARRAY);
}
float bank(float z){return -3.5f+.65f*std::sin(z*.16f)+.35f*std::sin(z*.53f+.8f)+.12f*std::sin(z*1.4f);}
float terrainHeight(float x,float z){
    float f=glm::clamp((x+18)/(bank(z)+18),0.f,1.f);
    return .12f+.48f*(1-f)+.10f*std::sin(z*.3f)*(1-f)-.18f*std::pow(f,10.f);
}
void vertex(float x,float y,float z,float u,float v){glTexCoord2f(u,v);glVertex3f(x,y,z);}
GLuint groundList=0;
void buildGround(){
    groundList=glGenLists(1);glNewList(groundList,GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for(int iz=0;iz<440;++iz){
        float za=-70+iz*.25f,zb=za+.25f;
        for(int j=0;j<14;++j){
            auto p=[&](int k,float z){float f=k/14.0f;return glm::vec3(-18+(bank(z)+18)*f,.12f+.48f*(1-f)+.10f*std::sin(z*.3f)*(1-f)-.18f*std::pow(f,10.f),z);};
            auto a=p(j,za),b=p(j,zb),c=p(j+1,zb),d=p(j+1,za);
            auto tri=[&](glm::vec3 v0,glm::vec3 v1,glm::vec3 v2){
                auto n=glm::normalize(glm::cross(v1-v0,v2-v0));glNormal3fv(glm::value_ptr(n));
                for(auto v:{v0,v1,v2})vertex(v.x,v.y,v.z,v.x*.4f,v.z*.4f);
            };
            tri(a,b,c);tri(a,c,d);
        }
    }
    glEnd();glEndList();
}
void drawWater(){
    material(glm::vec3(1),1);glNormal3f(0,1,0);
    glBegin(GL_QUADS);
    vertex(-70,0,-80,-7,-8);vertex(-70,0,50,-7,5);vertex(70,0,50,7,5);vertex(70,0,-80,7,-8);
    glEnd();
}
// Each ridge is a continuous terrain patch, with depth and curved silhouettes.
std::vector<SceneModel> buildMountains(){
    std::vector<SceneModel> models;
    for(int row=2;row>=0;--row){
        float centerZ=-29-row*17.f;
        auto height=[&](float x,float z){
            float depth=std::exp(-std::pow((z-centerZ)/8.f,2.f));
            float peaks=4.3f*std::exp(-std::pow((x+21-row*4)/9.f,2.f))
                +6.7f*std::exp(-std::pow((x-26+row*8)/13.f,2.f))
                +2.7f*std::exp(-std::pow((x+45)/11.f,2.f));
            return -.3f+depth*peaks*(.97f+.025f*std::sin(x*.34f+z*.18f)+.008f*std::sin(x*1.1f));
        };
        std::vector<SceneVertex> vertices;vertices.reserve(28*140*6);
        for(int iz=0;iz<28;++iz)for(int ix=0;ix<140;++ix){
            float x=-70+ix,z=centerZ-14+iz;
            auto p=[&](float xx,float zz){return glm::vec3(xx,height(xx,zz),zz);};
            auto tri=[&](glm::vec3 a,glm::vec3 b,glm::vec3 c){
                for(auto v:{a,b,c}){
                    float dx=(height(v.x+.15f,v.z)-height(v.x-.15f,v.z))/.3f;
                    float dz=(height(v.x,v.z+.15f)-height(v.x,v.z-.15f))/.3f;
                    auto n=glm::normalize(glm::vec3(-dx,1,-dz));vertices.push_back({v,n,glm::vec2(v.x*.12f,v.z*.12f)});
                }
            };
            tri(p(x,z),p(x,z+1),p(x+1,z+1));tri(p(x,z),p(x+1,z+1),p(x+1,z));
        }
        GpuBatch batch;batch.kind=4;batch.color=glm::vec3(.16f+row*.13f,.31f+row*.105f,.35f+row*.09f);batch.count=static_cast<GLsizei>(vertices.size());
        sceneGenBuffers(1,&batch.buffer);sceneBindBuffer(SCENE_ARRAY_BUFFER,batch.buffer);
        sceneBufferData(SCENE_ARRAY_BUFFER,vertices.size()*sizeof(SceneVertex),vertices.data(),SCENE_STATIC_DRAW);sceneBindBuffer(SCENE_ARRAY_BUFFER,0);
        SceneModel model;model.batches.push_back(batch);model.triangles=vertices.size()/3;models.push_back(model);
    }
    return models;
}
SceneModel buildMeadow(){
    SceneModel model;
    auto random=[](float n){return glm::fract(std::sin(n*127.1f+31.7f)*43758.5453f);};
    for(int group=0;group<3;++group){
        std::vector<SceneVertex> vertices;
        for(int i=group;i<11000;i+=3){
            float x=-13+random(float(i)+1)*10,z=-12+random(float(i)+4)*22;
            if(x>bank(z)-.38f)continue;
            if(std::sin(x*1.7f+std::sin(z*.8f))+std::cos(z*1.3f)<-.45f)continue;
            if(glm::length(glm::vec2(x+4.3f,z-1))<.65f)continue;
            float y=terrainHeight(x,z);
            for(int blade=0;blade<6;++blade){
                float a=random(i*7.f+blade)*6.2831853f,h=.075f+.13f*random(i*9.f+blade);
                glm::vec3 side(std::cos(a)*.021f,0,std::sin(a)*.021f),root(x+.04f*std::sin(a),y,z+.04f*std::cos(a)),bend(std::sin(a)*.065f,0,std::cos(a)*.065f);
                auto center=[&](float t){return root+glm::vec3(0,h*t,0)+bend*t*t;};
                for(int k=0;k<2;++k){float t=k*.5f,u=t+.5f;auto A=center(t)-side*(1-t),B=center(t)+side*(1-t),C=center(u)+side*(1-u),D=center(u)-side*(1-u);
                    auto n=glm::normalize(glm::vec3(-std::sin(a)*.6f,.6f,std::cos(a)*.6f));
                    vertices.insert(vertices.end(),{{A,n,{0,t}},{B,n,{1,t}},{C,n,{1,u}},{A,n,{0,t}},{C,n,{1,u}},{D,n,{0,u}}});
                }
            }
        }
        GpuBatch batch;batch.kind=5;batch.color=group==0?glm::vec3(.22f,.34f,.19f):group==1?glm::vec3(.33f,.41f,.24f):glm::vec3(.18f,.28f,.19f);batch.count=static_cast<GLsizei>(vertices.size());
        sceneGenBuffers(1,&batch.buffer);sceneBindBuffer(SCENE_ARRAY_BUFFER,batch.buffer);sceneBufferData(SCENE_ARRAY_BUFFER,vertices.size()*sizeof(SceneVertex),vertices.data(),SCENE_STATIC_DRAW);sceneBindBuffer(SCENE_ARRAY_BUFFER,0);model.batches.push_back(batch);
    }return model;
}
void drawReeds(){
    for(int i=0;i<210;++i){
        float z=-15+i*.15f+.40f*std::sin(i*1.8f);float x=bank(z)-.12f-.75f*(.5f+.5f*std::sin(i*2.7f));
        if(std::sin(z*.9f)<-.1f)continue;
        float height=.16f+.23f*(.5f+.5f*std::sin(i*5.1f));
        material(i%3?glm::vec3(.32f,.38f,.23f):glm::vec3(.52f,.45f,.27f),3);
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        for(int blade=0;blade<3;++blade){
            float a=blade*2.09f+i;glm::vec3 side(.017f*std::cos(a),0,.017f*std::sin(a));
            for(int k=0;k<4;++k){
                float t0=k/4.f,t1=(k+1)/4.f;
                auto p=[&](float t){return glm::vec3(x+.13f*std::sin(a)*t*t,.15f+height*t,z+.10f*std::cos(a)*t*t);};
                auto a0=p(t0)-side*(1-t0),b0=p(t0)+side*(1-t0),a1=p(t1)-side*(1-t1),b1=p(t1)+side*(1-t1);
                for(auto v:{a0,b0,b1,a0,b1,a1})vertex(v.x,v.y,v.z,0,0);
            }
        }glEnd();
    }
}
void sky(int w,int h){
    pUseProgram(0);glDisable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    float horizon=glm::clamp(std::tan(glm::radians(pitch))/.4142f,-.9f,.8f);
    glBegin(GL_QUADS);
    glColor3f(.88f,.78f,.62f);glVertex2f(-1,-1);glVertex2f(1,-1);
    glColor3f(.90f,.79f,.65f);glVertex2f(1,horizon);glVertex2f(-1,horizon);
    glColor3f(.90f,.79f,.65f);glVertex2f(-1,horizon);glVertex2f(1,horizon);
    glColor3f(.38f,.53f,.59f);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
    // A distant sun follows a fixed world direction when the camera orbits.
    float a=glm::radians(yaw),b=glm::radians(pitch);
    glm::vec3 forward(-std::sin(a)*std::cos(b),-std::sin(b),-std::cos(a)*std::cos(b));
    glm::vec3 right(std::cos(a),0,-std::sin(a)),up(-std::sin(a)*std::sin(b),std::cos(b),-std::cos(a)*std::sin(b));
    glm::vec3 sunDirection=glm::normalize(glm::vec3(-.15f,.05f,-1));float depth=glm::dot(sunDirection,forward);
    if(depth>.1f){
        float x=glm::dot(sunDirection,right)/depth/ .4142f*float(h)/w,y=glm::dot(sunDirection,up)/depth/.4142f;
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        for(int ring=5;ring>=0;--ring){
            float radius=ring==0?.026f:.045f+ring*.018f;
            glColor4f(.99f,.79f,.52f,ring==0?.75f:.018f);glBegin(GL_TRIANGLE_FAN);glVertex2f(x,y);
            for(int j=0;j<=80;++j){float t=j*6.2831853f/80;glVertex2f(x+radius*std::cos(t)*float(h)/w,y+radius*std::sin(t));}glEnd();
        }glDisable(GL_BLEND);
    }
    glEnable(GL_DEPTH_TEST);
}
void resetView(){yaw=8;pitch=7;distanceToTarget=11;target=glm::vec3(-.8f,1.45f,-.6f);}
void mouseButton(GLFWwindow* w,int button,int action,int){
    if(button==GLFW_MOUSE_BUTTON_LEFT)leftDown=action==GLFW_PRESS;
    if(button==GLFW_MOUSE_BUTTON_RIGHT)rightDown=action==GLFW_PRESS;
    glfwGetCursorPos(w,&lastX,&lastY);
}
void mouseMove(GLFWwindow*,double x,double y){
    float dx=float(x-lastX),dy=float(y-lastY);lastX=x;lastY=y;
    if(leftDown){yaw-=dx*.25f;pitch=glm::clamp(pitch+dy*.22f,5.0f,80.0f);}
    if(rightDown){float a=glm::radians(yaw);target+=glm::vec3(-std::cos(a)*dx,dy*.45f,std::sin(a)*dx)*distanceToTarget*.0015f;}
}
void scroll(GLFWwindow*,double,double dy){distanceToTarget=glm::clamp(distanceToTarget*std::exp(float(-dy)*.1f),5.0f,65.0f);}
void keyboard(GLFWwindow* w,int key,int,int action,int){
    if(action!=GLFW_PRESS)return;
    if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
    if(key==GLFW_KEY_SPACE)paused=!paused;
    if(key==GLFW_KEY_P)markers=!markers;
    if(key==GLFW_KEY_H)showHelp=!showHelp;
    if(key==GLFW_KEY_R){resetView();animationTime=0;paused=false;}
    if(key==GLFW_KEY_F9)captureRequested=true;
    if(key==GLFW_KEY_1)resetView();
    if(key==GLFW_KEY_2){yaw=48;pitch=29;distanceToTarget=23;target=glm::vec3(-1,1,-3);}
    if(key==GLFW_KEY_3){yaw=4;pitch=10;distanceToTarget=6;target=farewellPosition(animationTime)+glm::vec3(0,1.4f,0);}
    if(key==GLFW_KEY_4){yaw=48;pitch=12;distanceToTarget=7;target=glm::vec3(-4.3f,1.55f,1);}
}
void saveFrame(const std::filesystem::path& path,int w,int h){
    int stride=(w*3+3)&~3;std::vector<unsigned char> data(size_t(stride)*h);
    glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,w,h,GL_BGR_EXT,GL_UNSIGNED_BYTE,data.data());
    BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+stride*h;
    ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=h;ih.biPlanes=1;ih.biBitCount=24;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&fh),sizeof(fh));out.write(reinterpret_cast<char*>(&ih),sizeof(ih));out.write(reinterpret_cast<char*>(data.data()),data.size());
    if(!out)throw std::runtime_error("Cannot write screenshot");
}
void marker(glm::vec3 p,glm::vec3 color){
    material(color);glNormal3f(0,1,0);glBegin(GL_TRIANGLE_FAN);vertex(p.x,p.y,p.z,0,0);
    for(int i=0;i<=40;++i){float a=i*6.2831853f/40;vertex(p.x+.4f*std::cos(a),p.y,p.z+.4f*std::sin(a),0,0);}glEnd();
}

int wmain(int argc,wchar_t** argv){
    SetConsoleOutputCP(CP_UTF8);
    try {
        wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);
        auto exe=std::filesystem::path(module).parent_path();std::filesystem::current_path(exe);
        for(auto candidate:{exe/"assets/luoshen-environment",exe.parent_path()/"assets/luoshen-environment",exe.parent_path().parent_path()/"assets/luoshen-environment"})
            if(std::filesystem::exists(candidate/"tree/tree_small_lod1.obj")){assetRoot=candidate;break;}
        if(assetRoot.empty())throw std::runtime_error("Asset folder not found beside executable or project.");
        bool capture=argc>1&&std::wstring(argv[1])==L"--capture";
        bool noShadows=argc>1&&std::wstring(argv[1])==L"--capture-no-shadows";
        capture=capture||noShadows;
        bool smoke=argc>1&&std::wstring(argv[1])==L"--smoke";
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
        glfwWindowHint(GLFW_SAMPLES,4);
        glfwWindowHint(GLFW_STENCIL_BITS,8);
        if(capture||smoke)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto window=glfwCreateWindow(1600,900,"Luoshen | 1/2/3/4 views | Space pause | R replay | P path | H text | F9 screenshot",nullptr,nullptr);
        if(!window)throw std::runtime_error("Window creation failed");
        glfwMakeContextCurrent(window);glfwSwapInterval(capture?0:1);
        glfwSetMouseButtonCallback(window,mouseButton);glfwSetCursorPosCallback(window,mouseMove);
        glfwSetScrollCallback(window,scroll);glfwSetKeyCallback(window,keyboard);
        glfwSetWindowFocusCallback(window,[](GLFWwindow*,int focus){if(!focus)leftDown=rightDown=false;});
        if(!initShaderFunctions())throw std::runtime_error("Shader functions unavailable");
        sceneGenBuffers=reinterpret_cast<GenBuffersFn>(glfwGetProcAddress("glGenBuffers"));
        sceneBindBuffer=reinterpret_cast<BindBufferFn>(glfwGetProcAddress("glBindBuffer"));
        sceneBufferData=reinterpret_cast<BufferDataFn>(glfwGetProcAddress("glBufferData"));
        sceneDeleteBuffers=reinterpret_cast<DeleteBuffersFn>(glfwGetProcAddress("glDeleteBuffers"));
        sceneActiveTexture=reinterpret_cast<ActiveTextureFn>(glfwGetProcAddress("glActiveTexture"));
        if(!sceneGenBuffers||!sceneBindBuffer||!sceneBufferData||!sceneDeleteBuffers)throw std::runtime_error("Vertex buffers unavailable");
        program=createPhongProgram();if(!program)throw std::runtime_error("Shader compilation/link failed");
        GLuint titleOverlay=makeSceneOverlay(),overlay=makeSceneOverlay(true);
        auto uniform=[&](const char* n){return pGetUniformLocation(program,n);};
        colorLocation=uniform("baseColor");textureLocation=uniform("useTexture");kindLocation=uniform("materialKind");timeLocation=uniform("sceneTime");lightLocation=uniform("lightPositionEye");
        reflectionLocation=uniform("reflectionPass");goddessLocation=uniform("goddessPosition");worldUpLocation=uniform("worldUpEye");cameraHeightLocation=uniform("cameraHeight");
        // pUniform1f is obtained like the other GLSL entry points in Assignment2.
        auto uniform1f=reinterpret_cast<void(APIENTRY*)(GLint,GLfloat)>(glfwGetProcAddress("glUniform1f"));
        if(!uniform1f)throw std::runtime_error("glUniform1f unavailable");
        pUseProgram(program);pUniform1i(uniform("texture0"),0);
        pUniform1i(uniform("normalMap"),1);
        glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        std::cout<<"GL version: "<<glGetString(GL_VERSION)<<"\nPhong shader linked successfully\n";
        ShadowMap shadows;shadows.init(program);
        auto boat=loadModel("boat-medieval/wooden-rowboat.obj");auto tree=loadModel("tree/tree_small_lod1.obj");
        auto caozhi=loadModel("../luoshen-characters/caozhi/caozhi.obj");
        auto luoshen=loadModel("../luoshen-characters/luoshen/luoshen.obj");
        SceneMaterial groundMat;groundMat.colorMap=assetRoot/"ground/color.png";groundColor=loadMaterialTexture(groundMat);
        groundMat.colorMap=assetRoot/"ground/normal.png";groundNormal=loadMaterialTexture(groundMat);
        sceneActiveTexture(0x84C1);glBindTexture(GL_TEXTURE_2D,groundNormal);sceneActiveTexture(0x84C0);
        std::vector<SceneModel> rocks;for(int i=1;i<=6;++i){char f[64];std::snprintf(f,sizeof(f),"rocks/rock_%02d.obj",i);rocks.push_back(loadModel(f));}
        buildGround();auto mountainModels=buildMountains();auto meadow=buildMeadow();
        auto checkGL=[&](const char* label){GLenum err=glGetError();std::cout<<label<<" GL error="<<err<<'\n';if(err)throw std::runtime_error("OpenGL error");};
        checkGL("Assets uploaded");
        // Exercise the same callback handlers used by real mouse/keyboard input.
        auto originalYaw=yaw;leftDown=true;lastX=0;lastY=0;mouseMove(window,30,12);leftDown=false;
        if(yaw==originalYaw)throw std::runtime_error("Orbit control failed");
        auto beforeZoom=distanceToTarget;scroll(window,0,2);
        if(distanceToTarget>=beforeZoom)throw std::runtime_error("Zoom control failed");
        keyboard(window,GLFW_KEY_SPACE,0,GLFW_PRESS,0);
        if(!paused)throw std::runtime_error("Pause control failed");
        animationTime=17;keyboard(window,GLFW_KEY_R,0,GLFW_PRESS,0);
        if(animationTime!=0||paused)throw std::runtime_error("Replay must restart even when paused");
        std::cout<<"Control callbacks: orbit, zoom, pause/reset PASS\n";
        std::cout<<"CONTROLS: left orbit, right pan, wheel zoom; WASD pan; 1/2/3/4 view; P path; H text; Space pause; R replay; F9 screenshot\n";
        double previous=glfwGetTime(),start=previous;int frame=0;size_t frames=0;double fpsStart=previous;
        while(!glfwWindowShouldClose(window)){
            glfwPollEvents();double now=glfwGetTime();float dt=glm::clamp(float(now-previous),0.0f,.05f);previous=now;
            if(!paused)animationTime+=dt;
            if(capture){const double times[]={0,0,0,0,22.5,45};animationTime=times[frame];}
            float speed=dt*distanceToTarget*.15f;
            if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS)target.x-=speed;
            if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS)target.x+=speed;
            if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS)target.z-=speed;
            if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS)target.z+=speed;
            int w,h;glfwGetFramebufferSize(window,&w,&h);if(w==0||h==0){glfwWaitEvents();continue;}
            glViewport(0,0,w,h);glStencilMask(0xff);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);sky(w,h);
            auto projection=glm::perspective(glm::radians(45.0f),float(w)/h,.1f,160.0f);
            float a=glm::radians(yaw),b=glm::radians(pitch);
            glm::vec3 eye=target+distanceToTarget*glm::vec3(std::sin(a)*std::cos(b),std::sin(b),std::cos(a)*std::cos(b));
            auto view=glm::lookAt(eye,target,glm::vec3(0,1,0));
            auto goddess=farewellPosition(animationTime);goddess.y+=.012f*std::sin(float(animationTime)*1.3f);
            auto drawCasters=[&](const glm::mat4& view){
            // Position / yaw / uniform scale are intentionally explicit for students.
            drawModel(tree,view,glm::vec3(-8.0f,terrainHeight(-8,-3.5f),-3.5f),-28,1.5f);
            drawModel(tree,view,glm::vec3(-10.3f,terrainHeight(-10.3f,-10.5f),-10.5f),54,1.18f,glm::vec3(.94f,1,.94f));
            for(int i=0;i<12;++i){
                int group=i/4;float z=(group==0?-10.f:group==1?-1.f:7.f)+.9f*std::sin(i*2.1f);float x=bank(z)-.30f+.65f*std::sin(i*1.9f);
                drawModel(rocks[i%6],view,glm::vec3(x,-.04f,z),float(i*43),.18f+.27f*float((i*7)%5)/4);
            }
            drawModel(rocks[3],view,glm::vec3(-5.0f,terrainHeight(-5,3.6f)-.09f,3.6f),-30,.68f);
            drawModel(rocks[1],view,glm::vec3(-7.8f,terrainHeight(-7.8f,5.5f)-.07f,5.5f),68,.61f);
            float bob=.018f*std::sin(float(animationTime)*.85f);
            // The boat is empty background scenery. Luo Shen moves independently.
            drawModel(boat,view,glm::vec3(-2.3f,.025f+bob,-15.5f),-24,.36f);
            drawModel(caozhi,view,glm::vec3(-4.3f,terrainHeight(-4.3f,1)+.006f,1.0f),15,1.6f);
            drawModel(luoshen,view,goddess,-25,1.6f);
            };
            pUseProgram(program);uniform1f(timeLocation,float(animationTime));shadows.begin();drawCasters(shadows.lightView);shadows.end(w,h,view,!noShadows);
            glMatrixMode(GL_PROJECTION);glLoadMatrixf(glm::value_ptr(projection));
            pUseProgram(program);uniform1f(timeLocation,float(animationTime));
            auto worldUp=glm::vec3(view*glm::vec4(0,1,0,0));pUniform3f(worldUpLocation,worldUp.x,worldUp.y,worldUp.z);uniform1f(cameraHeightLocation,eye.y);
            glm::vec3 light=glm::vec3(view*glm::vec4(-12,16,9,1));pUniform3f(lightLocation,light.x,light.y,light.z);

            pUniform3f(goddessLocation,goddess.x,goddess.y,goddess.z);
            glMatrixMode(GL_MODELVIEW);glLoadMatrixf(glm::value_ptr(view));
            // Stencil confines planar reflected geometry to the river's screen area.
            glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_FALSE);drawWater();
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
            glStencilMask(0);glStencilFunc(GL_EQUAL,1,0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);reflectionPass=true;
            drawModel(caozhi,view,glm::vec3(-4.3f,terrainHeight(-4.3f,1)+.006f,1),15,1.6f);
            drawModel(luoshen,view,goddess,-25,1.6f);
            drawModel(tree,view,glm::vec3(-8.0f,terrainHeight(-8,-3.5f),-3.5f),-28,1.5f);
            reflectionPass=false;glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);
            glLoadMatrixf(glm::value_ptr(view));glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);drawWater();glDisable(GL_BLEND);
            for(const auto& mountain:mountainModels)drawModel(mountain,view,glm::vec3(0),0,1);material(glm::vec3(.85f,.90f,.81f),2,groundColor);
            glCallList(groundList);
            glLoadMatrixf(glm::value_ptr(view));drawReeds();drawModel(meadow,view,glm::vec3(0),0,1);
            drawCasters(view);
            glLoadMatrixf(glm::value_ptr(view));
            if(markers){
                marker(glm::vec3(-4.3f,.23f,1),glm::vec3(.2f,.3f,.5f));
                marker(goddess,glm::vec3(.8f,.5f,.25f));
                material(glm::vec3(.85f,.65f,.3f));glLineWidth(2);glBegin(GL_LINE_STRIP);
                for(int i=0;i<=100;++i){auto p=farewellPosition(i*.45);glVertex3f(p.x,.07f,p.z);}glEnd();
            }
            drawSceneOverlay(showHelp?overlay:titleOverlay,showHelp);
            if(capture){
                auto folder=exe/"captures";std::filesystem::create_directories(folder);
                const char* names[]={"scene-main.bmp","scene-overview.bmp","scene-luoshen.bmp","scene-caozhi.bmp","scene-departure.bmp","scene-farewell.bmp"};
                saveFrame(folder/names[frame],w,h);
                checkGL("Frame rendered");
                if(frame==0)keyboard(window,GLFW_KEY_2,0,GLFW_PRESS,0);
                if(frame==1)keyboard(window,GLFW_KEY_3,0,GLFW_PRESS,0);
                if(frame==2)keyboard(window,GLFW_KEY_4,0,GLFW_PRESS,0);
                if(frame==3){resetView();markers=false;}
                if(++frame>=6)break;
            }
            if(captureRequested){saveFrame(exe/"screenshot.bmp",w,h);captureRequested=false;}
            if(smoke&&frame<3){
                // Drivers may compile specialised shader variants on first draw.
                // Measure six seconds after that work, rather than exiting on it.
                glFinish();now=glfwGetTime();
                if(++frame==3){std::cout<<"Render warmup="<<now-start<<"s\n";start=previous=fpsStart=now;frames=0;}
            }
            glfwSwapBuffers(window);++frames;
            if(smoke&&frame>=3&&now-start>6.0){checkGL("Smoke run");std::cout<<"Smoke elapsed="<<now-start<<"s\n";break;}
            if(now-fpsStart>2.0){std::cout<<"FPS "<<frames/(now-fpsStart)<<'\n';frames=0;fpsStart=now;}
        }
        checkGL("Shutdown");
        for(auto& item:textures)glDeleteTextures(1,&item.second);
        glDeleteTextures(1,&overlay);
        glDeleteTextures(1,&titleOverlay);
        auto release=[](SceneModel& m){for(auto b:m.batches)sceneDeleteBuffers(1,&b.buffer);};
        release(boat);release(tree);release(caozhi);release(luoshen);release(meadow);for(auto& mountain:mountainModels)release(mountain);for(auto& r:rocks)release(r);glDeleteLists(groundList,1);
        shadows.destroy();pDeleteProgram(program);glfwDestroyWindow(window);glfwTerminate();CoUninitialize();
        return 0;
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
