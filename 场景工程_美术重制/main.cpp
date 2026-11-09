// 洛水别绪：曹植与洛神的河岸离别，OBJ贴图、Phong、倒影与Bézier动画。
#include "graphics_support.h"
#include "mesh_data.h"
#include "scene_layout.h"
#include <map>
#include <chrono>
#include <cstddef>
#include <cstring>
#include "scene_overlay.h"
#include "shadow_map.h"
#include "layout_editor.h"
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
struct SceneModel {std::vector<GpuBatch> batches;glm::vec3 center{0},size{0};float bottom=0;size_t triangles=0;std::vector<glm::vec3> pickTriangles;};
GLuint program=0;
GLint colorLocation=-1,textureLocation=-1,kindLocation=-1,timeLocation=-1,lightLocation=-1;
GLint reflectionLocation=-1,goddessLocation=-1,worldUpLocation=-1,cameraHeightLocation=-1;
using ActiveTextureFn=void(APIENTRY*)(GLenum);
ActiveTextureFn sceneActiveTexture=nullptr;
bool reflectionPass=false,shadowPass=false,showHelp=false;
bool animationPlaying=true;
double animationTime=0.0;
glm::mat4 worldPrefix(1);
GLuint groundColor=0,groundNormal=0;
std::map<std::string,GLuint> textures;
std::filesystem::path assetRoot;
bool captureRequested=false;
float yaw=scene_layout::mainYaw,pitch=scene_layout::mainPitch,distanceToTarget=scene_layout::mainDistance;
glm::vec3 target=scene_layout::mainTarget;
bool leftDown=false,rightDown=false;double lastX=0,lastY=0;
LayoutEditor layoutEditor;
size_t caozhiItem=0,luoshenItem=0;

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

SceneModel loadModel(const std::string& relative,bool pickable=false){
    auto mesh=readSceneObj(assetRoot/std::filesystem::u8path(relative));
    SceneModel model;model.center=(mesh.minimum+mesh.maximum)*.5f;model.size=mesh.maximum-mesh.minimum;model.bottom=mesh.minimum.y;model.triangles=mesh.triangles;
    for(const auto& batch:mesh.batches){
        if(pickable){
            auto pivot=glm::vec3(model.center.x,model.bottom,model.center.z);
            for(const auto& vertex:batch.vertices)model.pickTriangles.push_back(vertex.position-pivot);
        }
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
void drawModel(const SceneModel& model,const glm::mat4& view,glm::vec3 position,float angle,float scale,glm::vec3 tint=glm::vec3(1),glm::vec3 stretch=glm::vec3(1),glm::vec2 tilt=glm::vec2(0)){
    auto m=glm::translate(worldPrefix*(reflectionPass?glm::scale(glm::mat4(1),glm::vec3(1,-1,1)):glm::mat4(1)),position);
    m=glm::rotate(m,glm::radians(angle),glm::vec3(0,1,0));
    m=glm::rotate(m,glm::radians(tilt.x),glm::vec3(1,0,0));
    m=glm::rotate(m,glm::radians(tilt.y),glm::vec3(0,0,1));m=glm::scale(m,stretch*scale);
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
float rockReach(float z){return glm::smoothstep(-45.f,-36.f,z)*(1.f-glm::smoothstep(-9.f,-6.f,z));}
float bank(float z){
    return -3.5f+.65f*std::sin(z*.16f)+.35f*std::sin(z*.53f+.8f)+.12f*std::sin(z*1.4f)
           +rockReach(z)*(.38f+.28f*std::sin(z*.41f))
           -.90f*std::exp(-std::pow((z+9.8f)/2.1f,2.f));
}
float terrainHeight(float x,float z){
    float f=glm::clamp((x+18)/(bank(z)+18),0.f,1.f);
    float base=.12f+.48f*(1-f)+.10f*std::sin(z*.3f)*(1-f)-.18f*std::pow(f,10.f);
    float inland=bank(z)-x;
    float shoulder=glm::smoothstep(0.f,1.9f,inland)*(.30f+.13f*std::sin(z*.62f));
    float bluff=glm::smoothstep(1.8f,7.2f,inland)*(.75f+.35f*std::sin(z*.31f+.8f));
    return base+rockReach(z)*(shoulder+bluff);
}
void vertex(float x,float y,float z,float u,float v){glTexCoord2f(u,v);glVertex3f(x,y,z);}
GLuint groundList=0;
GLuint bankLists=0;
void groundTriangle(glm::vec3 a,glm::vec3 b,glm::vec3 c){
    auto n=glm::normalize(glm::cross(b-a,c-a));
    glNormal3fv(glm::value_ptr(n));
    for(auto p:{a,b,c})vertex(p.x,p.y,p.z,p.x*.4f,p.z*.4f);
}
void buildGround(){
    groundList=glGenLists(1);glNewList(groundList,GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for(int iz=0;iz<440;++iz){
        float za=-70+iz*.25f,zb=za+.25f;
        if(za>=-45.f&&zb<=-6.f)continue;
        for(int j=0;j<14;++j){
            auto p=[&](int k,float z){float x=glm::mix(-18.f,bank(z),k/14.f);return glm::vec3(x,terrainHeight(x,z),z);};
            auto a=p(j,za),b=p(j,zb),c=p(j+1,zb),d=p(j+1,za);
            groundTriangle(a,b,c);groundTriangle(a,c,d);
        }
    }
    glEnd();glEndList();
    // Irregular cross sections give the shelf a broken rock edge and a broader inland shoulder.
    constexpr float sections[]={-45,-42.4f,-39.8f,-37.5f,-34.7f,-32.8f,-30.1f,-27.9f,-25.2f,
                                -23.7f,-21.5f,-19.1f,-17.4f,-15.2f,-13.9f,-12.1f,-10.6f,-9.5f,-8.1f,-7,-6};
    constexpr float bands[]={0,.17f,.35f,.54f,.68f,.80f,.88f,.94f,.975f,1};
    auto point=[&](int row,int col){
        float z=sections[row],f=bands[col];
        if(col>0&&col<9)f+=.018f*std::sin(row*1.91f+col*2.75f);
        float x=glm::mix(-18.f,bank(z),f);
        return glm::vec3(x,terrainHeight(x,z),z);
    };
    bankLists=glGenLists(3);
    for(int tone=0;tone<3;++tone){
        glNewList(bankLists+tone,GL_COMPILE);glBegin(GL_TRIANGLES);
        for(int row=0;row+1<int(std::size(sections));++row){
            for(int col=0;col+1<int(std::size(bands));++col){
                auto a=point(row,col),b=point(row+1,col),c=point(row+1,col+1),d=point(row,col+1);
                int faceTone=(row*7+col*11+row*col)%3;
                if(faceTone==tone)groundTriangle(a,b,c);
                if((faceTone+1+(row+col)%2)%3==tone)groundTriangle(a,c,d);
            }
        }
        glEnd();glEndList();
    }
}
void drawWater(){
    material(glm::vec3(1),1);glNormal3f(0,1,0);
    glBegin(GL_QUADS);
    vertex(-70,0,-80,-7,-8);vertex(-70,0,50,-7,5);vertex(70,0,50,7,5);vertex(70,0,-80,7,-8);
    glEnd();
}
constexpr int meadowPatchCount=4;
SceneModel buildMeadow(int patch){
    SceneModel model;
    glm::vec3 minimum(1000),maximum(-1000);
    auto random=[](float n){return glm::fract(std::sin(n*127.1f+31.7f)*43758.5453f);};
    for(int group=0;group<3;++group){
        std::vector<SceneVertex> vertices;
        for(int i=group;i<2800;i+=3){
            float x=-13+random(float(i)+1)*10,z=-12+random(float(i)+4)*22;
            if(int((z+12)*meadowPatchCount/22)!=patch)continue;
            if(x>bank(z)-.75f)continue;
            if(rockReach(z)>.15f&&random(float(i)+23)<rockReach(z)*.96f)continue;
            if(std::sin(x*1.7f+std::sin(z*.8f))+std::cos(z*1.3f)<-.45f)continue;
            if(glm::length(glm::vec2(x-scene_layout::caozhiPosition.x,z-scene_layout::caozhiPosition.z))<1.0f)continue;
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
        for(const auto& vertex:vertices){
            minimum=glm::min(minimum,vertex.position);
            maximum=glm::max(maximum,vertex.position);
            model.pickTriangles.push_back(vertex.position);
        }
        GpuBatch batch;batch.kind=5;batch.color=group==0?glm::vec3(.22f,.34f,.19f):group==1?glm::vec3(.33f,.41f,.24f):glm::vec3(.18f,.28f,.19f);batch.count=static_cast<GLsizei>(vertices.size());
        sceneGenBuffers(1,&batch.buffer);sceneBindBuffer(SCENE_ARRAY_BUFFER,batch.buffer);sceneBufferData(SCENE_ARRAY_BUFFER,vertices.size()*sizeof(SceneVertex),vertices.data(),SCENE_STATIC_DRAW);sceneBindBuffer(SCENE_ARRAY_BUFFER,0);model.batches.push_back(batch);
    }
    model.center=(minimum+maximum)*.5f;
    model.bottom=minimum.y;
    model.size=maximum-minimum;
    for(auto& point:model.pickTriangles)point-=glm::vec3(model.center.x,model.bottom,model.center.z);
    return model;
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
void resetView(){yaw=scene_layout::mainYaw;pitch=scene_layout::mainPitch;distanceToTarget=scene_layout::mainDistance;target=scene_layout::mainTarget;}
void mouseButton(GLFWwindow* w,int button,int action,int){
    if(button==GLFW_MOUSE_BUTTON_LEFT)leftDown=action==GLFW_PRESS;
    if(button==GLFW_MOUSE_BUTTON_RIGHT)rightDown=action==GLFW_PRESS;
    glfwGetCursorPos(w,&lastX,&lastY);
    if(layoutEditor.active&&button==GLFW_MOUSE_BUTTON_LEFT){
        if(leftDown)layoutEditor.click(w,{float(lastX),float(lastY)});
        else layoutEditor.endDrag();
    }
}
void mouseMove(GLFWwindow* w,double x,double y){
    float dx=float(x-lastX),dy=float(y-lastY);lastX=x;lastY=y;
    if(leftDown&&layoutEditor.active){
        bool fine=glfwGetKey(w,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(w,GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS;
        layoutEditor.drag(w,{float(x),float(y)},fine);
    }else if(leftDown){yaw-=dx*.25f;pitch=glm::clamp(pitch+dy*.22f,5.0f,80.0f);}
    if(rightDown){float a=glm::radians(yaw);target+=glm::vec3(-std::cos(a)*dx,dy*.45f,std::sin(a)*dx)*distanceToTarget*.0015f;}
}
void scroll(GLFWwindow*,double,double dy){distanceToTarget=glm::clamp(distanceToTarget*std::exp(float(-dy)*.1f),5.0f,65.0f);}
void keyboard(GLFWwindow* w,int key,int,int action,int mods){
    if(layoutEditor.key(w,key,action,mods))return;
    if(action!=GLFW_PRESS)return;
    if(key==GLFW_KEY_TAB){
        layoutEditor.active=!layoutEditor.active;
        leftDown=rightDown=false;
        layoutEditor.endDrag();
        layoutEditor.title(w);
    }
    if(key==GLFW_KEY_ESCAPE)glfwSetWindowShouldClose(w,1);
    if(key==GLFW_KEY_H)showHelp=!showHelp;
    if(key==GLFW_KEY_SPACE)animationPlaying=!animationPlaying;
    if(key==GLFW_KEY_R){animationTime=0.0;animationPlaying=true;resetView();}
    if(key==GLFW_KEY_F9)captureRequested=true;
    if(key==GLFW_KEY_1)resetView();
    if(key==GLFW_KEY_2){yaw=48;pitch=29;distanceToTarget=23;target=glm::vec3(-1,1,-3);}
    if(key==GLFW_KEY_3){yaw=4;pitch=10;distanceToTarget=6;target=layoutEditor.items[luoshenItem].pose.position+glm::vec3(0,1.4f,0);}
    if(key==GLFW_KEY_4){yaw=48;pitch=11;distanceToTarget=4.2f;target=layoutEditor.items[caozhiItem].pose.position+glm::vec3(0,1.2f,0);}
}
void saveFrame(const std::filesystem::path& path,int w,int h){
    int stride=(w*3+3)&~3;std::vector<unsigned char> data(size_t(stride)*h);
    glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,w,h,GL_BGR_EXT,GL_UNSIGNED_BYTE,data.data());
    BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+stride*h;
    ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=h;ih.biPlanes=1;ih.biBitCount=24;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&fh),sizeof(fh));out.write(reinterpret_cast<char*>(&ih),sizeof(ih));out.write(reinterpret_cast<char*>(data.data()),data.size());
    if(!out)throw std::runtime_error("Cannot write screenshot");
}
#include "environment_effects.h"

int wmain(int argc,wchar_t** argv){
    SetConsoleOutputCP(CP_UTF8);
    try {
        wchar_t module[32768];GetModuleFileNameW(nullptr,module,32768);
        auto exe=std::filesystem::path(module).parent_path();std::filesystem::current_path(exe);
        for(auto candidate:{exe/"assets/luoshen-environment",exe.parent_path()/"assets/luoshen-environment",exe.parent_path().parent_path()/"assets/luoshen-environment"})
            if(std::filesystem::exists(candidate/"boat-medieval/wooden-rowboat.obj")){assetRoot=candidate;break;}
        if(assetRoot.empty())throw std::runtime_error("Asset folder not found beside executable or project.");
        bool capture=argc>1&&std::wstring(argv[1])==L"--capture";
        bool noShadows=argc>1&&std::wstring(argv[1])==L"--capture-no-shadows";
        capture=capture||noShadows;
        const int bankRounds=capture&&argc==4&&std::wstring(argv[2])==L"--bank-round"
            ?std::clamp(std::stoi(argv[3]),0,scene_layout::leftBankRounds):scene_layout::leftBankRounds;
        bool smoke=argc>1&&std::wstring(argv[1])==L"--smoke";
        bool environmentDemo=argc>1&&std::wstring(argv[1])==L"--environment-demo";
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
        glfwWindowHint(GLFW_SAMPLES,4);
        glfwWindowHint(GLFW_STENCIL_BITS,8);
        if(capture||smoke)glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto window=glfwCreateWindow(1600,900,"Luoshen | 1/2/3/4 views | R reset view | H text | F9 screenshot",nullptr,nullptr);
        if(!window)throw std::runtime_error("Window creation failed");
        glfwMakeContextCurrent(window);glfwSwapInterval(capture?0:1);
        glfwSetMouseButtonCallback(window,mouseButton);glfwSetCursorPosCallback(window,mouseMove);
        glfwSetScrollCallback(window,scroll);glfwSetKeyCallback(window,keyboard);
        glfwSetWindowFocusCallback(window,[](GLFWwindow*,int focus){if(!focus){leftDown=rightDown=false;layoutEditor.endDrag();}});
        layoutEditor.title(window);
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
        auto prop=[](const char* name,bool pickable=false){return loadModel(std::string("imported-props/v6/")+name+".obj",pickable);};
        auto boat=prop("05_covered_wooden_boat",true);
        auto blossom=prop("01_foreground_blossom_tree",true);
        auto ledge=prop("03_left_bank_platform_steps");
        auto leftRidge=prop("07_left_ridge_facets");
        auto rightSummit=prop("07_right_summit");
        auto valleyRidge=prop("07_river_valley_ridge");
        auto distantBank=prop("04_right_bank_slope");
        SceneModel pines[]={prop("02_pine_tree_A",true),prop("02_pine_tree_B",true),prop("02_pine_tree_C",true)};
        SceneModel rocks[]={prop("03_shore_rock_A",true),prop("03_shore_rock_B",true),prop("03_shore_rock_C",true)};
        SceneModel reeds[]={prop("06_shore_reeds_A",true),prop("06_shore_reeds_B",true),prop("06_shore_reeds_C",true)};
        SceneModel mountains[]={prop("07_mountain_far"),prop("07_mountain_mid"),prop("07_mountain_near")};
        SceneModel petals[]={prop("08_floating_petal_A"),prop("08_floating_petal_B"),prop("08_floating_petal_C")};
        auto caozhi=loadModel("../luoshen-characters/caozhi/caozhi.obj",true);
        auto luoshen=loadModel("../luoshen-characters/luoshen/luoshen.obj",true);
        struct EditableModel {const SceneModel* model;glm::vec3 stretch;};
        std::vector<EditableModel> editableModels;
        auto editable=[&](const SceneModel& model,std::string id,std::string name,
                          LayoutEditor::Pose pose,glm::vec3 stretch=glm::vec3(1),bool waterLocked=false){
            editableModels.push_back({&model,stretch});
            layoutEditor.items.push_back({id,name,pose,model.size*stretch,waterLocked,&model.pickTriangles,stretch});
        };
        const auto boatItem=layoutEditor.items.size();
        editable(boat,"boat",u8"小舟",{scene_layout::boatPosition,scene_layout::boatYaw,scene_layout::boatScale},glm::vec3(1),true);
        const auto blossomItem=layoutEditor.items.size();
        editable(blossom,"blossom",u8"桃花树",{scene_layout::blossomPosition,scene_layout::blossomYaw,scene_layout::blossomScale},scene_layout::blossomStretch);
        for(int i=1;i<int(scene_layout::ledges.size());++i){
            const auto& rock=scene_layout::ledges[i];
            auto number=std::to_string(i);
            editable(rocks[i-1],"foreground-"+number,u8"前景岩石 "+number,
                     {rock.position+glm::vec3(0,.20f,0),rock.yaw,rock.scale*1.4f});
        }
        const auto& connectors=scene_layout::shoreConnectors;
        std::array<SceneModel, scene_layout::shoreConnectors.size()> connectorModels;
        for(size_t i=0;i<connectors.size();++i){
            const auto& rock=connectors[i];
            connectorModels[i]=loadModel(std::string("imported-props/riverbank-rocks/")+rock.asset+".obj",true);
            editable(connectorModels[i],"shore-connector-"+std::to_string(i+1),
                     u8"左岸接坡石 "+std::to_string(i+1),{rock.pose.position,rock.pose.yaw,rock.pose.scale});
        }
        for(int i=0;i<int(scene_layout::bankRocks.size());++i){
            const auto& rock=scene_layout::bankRocks[i];
            auto number=std::to_string(i+1);
            auto position=rock.position;
            if(position.x<0&&position.y==0)position.y=terrainHeight(position.x,position.z)-.05f;
            editable(rocks[rock.variant],"bank-"+number,(rock.position.x<0?u8"左岸岩石 ":u8"右岸岩石 ")+number,
                     {position,rock.yaw,1.f},rock.size);
        }
        for(size_t i=0;i<scene_layout::riverPines.size();++i){
            const auto& pine=scene_layout::riverPines[i];
            auto position=pine.position;
            position.y=position.x<0?terrainHeight(position.x,position.z)-.015f:position.y-.09f;
            auto number=std::to_string(i+1);
            editable(pines[i%3],"pine-"+number,u8"松树 "+number,{position,pine.yaw,pine.scale});
        }
        for(size_t i=0;i<scene_layout::leftGrove.size();++i){
            const auto& pine=scene_layout::leftGrove[i];
            auto position=pine.position;
            position.y=terrainHeight(position.x,position.z)-.015f;
            auto number=std::to_string(i+1);
            editable(pines[(i+2)%3],"left-grove-"+number,u8"左岸远松 "+number,{position,pine.yaw,pine.scale});
        }
        for(size_t i=0;i<std::size(scene_layout::leftBankDetails);++i){
            const auto& detail=scene_layout::leftBankDetails[i];
            if(detail.round>bankRounds)break;
            const float y=terrainHeight(detail.x,detail.z)-(detail.pine?.045f:.09f);
            const auto& model=detail.pine?pines[detail.variant]:rocks[detail.variant];
            const auto number=std::to_string(i+1);
            editable(model,"left-detail-"+number,(detail.pine?u8"左岸补植松树 ":u8"左岸补置岩石 ")+number,
                     {{detail.x,y,detail.z},detail.yaw,1.f},detail.size);
        }
        for(int i=0;i<10;++i){
            float z=-13.f+1.55f*i;
            float x=bank(z)-.22f-.19f*(i%3);
            auto number=std::to_string(i+1);
            editable(reeds[i%3],"reeds-"+number,u8"岸边草丛 "+number,{{x,terrainHeight(x,z)-.015f,z},25.f*i,.58f});
        }
        caozhiItem=layoutEditor.items.size();
        editable(caozhi,"caozhi",u8"曹植",{scene_layout::caozhiPosition,scene_layout::caozhiYaw,scene_layout::caozhiScale});
        luoshenItem=layoutEditor.items.size();
        editable(luoshen,"luoshen",u8"洛神",{scene_layout::luoshenPosition,scene_layout::luoshenYaw,scene_layout::luoshenScale});
        const auto firstMeadowItem=editableModels.size();
        std::array<SceneModel,meadowPatchCount> meadow;
        for(int i=0;i<meadowPatchCount;++i){
            meadow[i]=buildMeadow(i);
            const auto& m=meadow[i];
            auto number=std::to_string(i+1);
            editable(m,"meadow-"+number,u8"草地草片 "+number,{{m.center.x,m.bottom,m.center.z},0,1});
        }
        layoutEditor.path=assetRoot.parent_path().parent_path()/"scene-layout.txt";
        if(capture&&argc==4&&std::wstring(argv[2])==L"--layout")
            layoutEditor.path=std::filesystem::path(argv[3]);
        layoutEditor.load();
        if(environmentDemo){yaw=-4.f;pitch=14.f;distanceToTarget=10.f;target=glm::vec3(-1.1f,1.2f,-3.4f);}
        SceneMaterial groundMat;groundMat.colorMap=assetRoot/"ground/color.png";groundColor=loadMaterialTexture(groundMat);
        groundMat.colorMap=assetRoot/"ground/normal.png";groundNormal=loadMaterialTexture(groundMat);
        sceneActiveTexture(0x84C1);glBindTexture(GL_TEXTURE_2D,groundNormal);sceneActiveTexture(0x84C0);
        buildGround();
        auto checkGL=[&](const char* label){GLenum err=glGetError();std::cout<<label<<" GL error="<<err<<'\n';if(err)throw std::runtime_error("OpenGL error");};
        checkGL("Assets uploaded");
        // Exercise the same callback handlers used by real mouse/keyboard input.
        auto originalYaw=yaw;leftDown=true;lastX=0;lastY=0;mouseMove(window,30,12);leftDown=false;
        if(yaw==originalYaw)throw std::runtime_error("Orbit control failed");
        const auto originalTarget=target;
        rightDown=true;mouseMove(window,48,26);rightDown=false;
        if(glm::length(target-originalTarget)<.001f)throw std::runtime_error("Pan control failed");
        auto beforeZoom=distanceToTarget;scroll(window,0,2);
        if(distanceToTarget>=beforeZoom)throw std::runtime_error("Zoom control failed");
        animationTime=2.0;keyboard(window,GLFW_KEY_SPACE,0,GLFW_PRESS,0);
        if(animationPlaying)throw std::runtime_error("Pause control failed");
        keyboard(window,GLFW_KEY_R,0,GLFW_PRESS,0);
        if(yaw!=scene_layout::mainYaw||distanceToTarget!=scene_layout::mainDistance||animationTime!=0.0||!animationPlaying)
            throw std::runtime_error("Reset control failed");
        if(environmentDemo){yaw=-4.f;pitch=14.f;distanceToTarget=10.f;target=glm::vec3(-1.1f,1.2f,-3.4f);}
        std::cout<<"Control callbacks: orbit, zoom, pause, reset PASS\n";
        double previous=glfwGetTime(),start=previous;int frame=0;size_t frames=0;double fpsStart=previous;
        while(!glfwWindowShouldClose(window)){
            glfwPollEvents();double now=glfwGetTime();float dt=glm::clamp(float(now-previous),0.0f,.05f);previous=now;
            if(animationPlaying)animationTime+=dt;
            float speed=dt*distanceToTarget*.15f;
            if(!layoutEditor.active){
            if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS)target.x-=speed;
            if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS)target.x+=speed;
            if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS)target.z-=speed;
            if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS)target.z+=speed;
            }
            int w,h;glfwGetFramebufferSize(window,&w,&h);if(w==0||h==0){glfwWaitEvents();continue;}
            glViewport(0,0,w,h);glStencilMask(0xff);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);sky(w,h);
            auto projection=glm::perspective(glm::radians(45.0f),float(w)/h,.1f,160.0f);
            float a=glm::radians(yaw),b=glm::radians(pitch);
            glm::vec3 eye=target+distanceToTarget*glm::vec3(std::sin(a)*std::cos(b),std::sin(b),std::cos(a)*std::cos(b));
            auto view=glm::lookAt(eye,target,glm::vec3(0,1,0));
            auto goddess=layoutEditor.items[luoshenItem].pose.position;
            auto drawEditable=[&](size_t i,const glm::mat4& cameraView){
                const auto& item=layoutEditor.items[i];
                const auto& entry=editableModels[i];
                drawModel(*entry.model,cameraView,item.pose.position,item.pose.yaw,item.pose.scale,glm::vec3(1),entry.stretch);
            };
            auto drawCasters=[&](const glm::mat4& view){
            for(size_t i=0;i<firstMeadowItem;++i){
                if(environmentDemo&&(i==caozhiItem||i==luoshenItem))continue;
                drawEditable(i,view);
            }
            drawModel(distantBank,view,scene_layout::distantBankPosition,0,1,glm::vec3(1),glm::vec3(1.55f,.42f,8.5f));
            drawModel(ledge,view,scene_layout::ledges[0].position,-8,1);
            };
            pUseProgram(program);uniform1f(timeLocation,static_cast<float>(animationTime));shadows.begin();drawCasters(shadows.lightView);shadows.end(w,h,view,!noShadows);
            glMatrixMode(GL_PROJECTION);glLoadMatrixf(glm::value_ptr(projection));
            pUseProgram(program);uniform1f(timeLocation,static_cast<float>(animationTime));
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
            drawCasters(view);
            reflectionPass=false;glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);
            glLoadMatrixf(glm::value_ptr(view));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            drawWater();
            glDisable(GL_BLEND);
            auto drawLeftBankRidges=[&](int variant){
                for(const auto& ridge:scene_layout::leftBankRidges){
                    if(ridge.round>bankRounds)break;
                    if(ridge.variant!=variant)continue;
                    auto position=ridge.position;
                    auto size=ridge.size;
                    float angle=ridge.yaw;
                    if(bankRounds>=41&&ridge.round==15){
                        position.x-=.55f;
                        position.z-=1.8f;
                        size.y*=.86f;
                    }
                    if(bankRounds>=42&&ridge.round==16){
                        position.x-=.42f;
                        angle-=16.f;
                        size.y*=.85f;
                    }
                    drawModel(mountains[ridge.variant],view,position,angle,1,{.84f,.91f,1},size);
                }
            };
            // Far mountains.
            drawModel(mountains[0],view,{-2,-.22f,-51},0,3.4f,{.84f,.91f,1});
            drawModel(mountains[0],view,scene_layout::leftFarPosition,0,1,{.84f,.91f,1},scene_layout::leftFarStretch);
            // Fog slot: between the far and middle mountains.
            drawModel(rightSummit,view,{9,-.20f,-36},-6,2.9f,{.84f,.91f,1});
            drawModel(leftRidge,view,scene_layout::leftMountainPosition,0,1,{.84f,.91f,1},scene_layout::leftMountainStretch);
            drawLeftBankRidges(1);
            // Fog slot: between the middle mountains and near foothills.
            drawModel(valleyRidge,view,{5.8f,-.24f,-26.0f},-7,1,{.84f,.91f,1});
            drawModel(mountains[2],view,scene_layout::leftFoothillPosition,0,1,{.84f,.91f,1},scene_layout::leftFoothillStretch);
            drawLeftBankRidges(2);
            glLoadMatrixf(glm::value_ptr(view));
            material(glm::vec3(.85f,.90f,.81f),2,groundColor);
            glCallList(groundList);
            const glm::vec3 bankColors[]={{.38f,.44f,.43f},{.45f,.49f,.46f},{.50f,.52f,.48f}};
            for(int tone=0;tone<3;++tone){material(bankColors[tone]);glCallList(bankLists+tone);}
            for(size_t i=firstMeadowItem;i<editableModels.size();++i)drawEditable(i,view);
            drawCasters(view);
            glLoadMatrixf(glm::value_ptr(view));
            environment_effects::drawWaterPetals(animationTime,goddess,layoutEditor.items[boatItem].pose.position,view,petals);
            environment_effects::drawFallingPetals(animationTime,layoutEditor.items[blossomItem].pose.position,view,petals);
            drawSceneOverlay(showHelp?overlay:titleOverlay,showHelp);
            int windowWidth,windowHeight;
            glfwGetWindowSize(window,&windowWidth,&windowHeight);
            layoutEditor.draw(projection*view,windowWidth,windowHeight);
            if(capture){
                auto folder=exe/"captures";std::filesystem::create_directories(folder);
                const char* names[]={"scene-main.bmp","scene-overview.bmp","scene-luoshen.bmp","scene-caozhi.bmp"};
                saveFrame(folder/names[frame],w,h);
                checkGL("Frame rendered");
                if(frame==0)keyboard(window,GLFW_KEY_2,0,GLFW_PRESS,0);
                if(frame==1)keyboard(window,GLFW_KEY_3,0,GLFW_PRESS,0);
                if(frame==2)keyboard(window,GLFW_KEY_4,0,GLFW_PRESS,0);
                if(++frame>=4)break;
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
        release(boat);release(blossom);release(ledge);release(distantBank);
        release(leftRidge);release(rightSummit);release(valleyRidge);
        release(caozhi);release(luoshen);
        for(auto& patch:meadow)release(patch);
        for(auto& m:pines)release(m);
        for(auto& m:rocks)release(m);
        for(auto& m:reeds)release(m);
        for(auto& m:mountains)release(m);
        for(auto& m:petals)release(m);
        glDeleteLists(groundList,1);
        glDeleteLists(bankLists,3);
        shadows.destroy();pDeleteProgram(program);glfwDestroyWindow(window);glfwTerminate();CoUninitialize();
        return 0;
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
