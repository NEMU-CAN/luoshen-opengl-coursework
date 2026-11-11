#pragma once
#include "scene_layout.h"
#include "layout_editor.h"
// Depth-map shadows; colour and normal textures keep units 0 and 1.
extern LayoutEditor layoutEditor;
struct ShadowMap {
    static constexpr int size=4096;
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using Status=GLenum(APIENTRY*)(GLenum);
    using Matrix=void(APIENTRY*)(GLint,GLsizei,GLboolean,const GLfloat*);
    using Active=void(APIENTRY*)(GLenum);
    GLuint framebuffer=0,depth=0;Gen gen=nullptr,remove=nullptr;Bind bind=nullptr;Attach attach=nullptr;Status status=nullptr;Matrix matrix=nullptr;Active active=nullptr;
    glm::mat4 lightView,lightProjection;
    GLint depthLocation=-1,matrixLocation=-1,enabledLocation=-1,kindLocation=-1,textureLocation=-1,cloudLocation=-1,rightLocation=-1,upLocation=-1,backLocation=-1,boatLocation=-1,footLocation=-1,goddessVelocityLocation=-1,boatVelocityLocation=-1;
    GLint cloudCameraLocation=-1;
    GLint trailLocation[16];
    int boatWakeEmits=0; // Diagnostics only: wakes raised by the drifting hull.
    void init(GLuint program){
        gen=reinterpret_cast<Gen>(glfwGetProcAddress("glGenFramebuffers"));bind=reinterpret_cast<Bind>(glfwGetProcAddress("glBindFramebuffer"));
        remove=reinterpret_cast<Gen>(glfwGetProcAddress("glDeleteFramebuffers"));attach=reinterpret_cast<Attach>(glfwGetProcAddress("glFramebufferTexture2D"));
        status=reinterpret_cast<Status>(glfwGetProcAddress("glCheckFramebufferStatus"));matrix=reinterpret_cast<Matrix>(glfwGetProcAddress("glUniformMatrix4fv"));active=reinterpret_cast<Active>(glfwGetProcAddress("glActiveTexture"));
        if(!gen||!bind||!remove||!attach||!status||!matrix||!active)throw std::runtime_error("Shadow functions unavailable");
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,0x81A6,size,size,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,0x884C,0x884E); // compare R to depth texture
        glTexParameteri(GL_TEXTURE_2D,0x884D,GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        gen(1,&framebuffer);bind(0x8D40,framebuffer);attach(0x8D40,0x8D00,GL_TEXTURE_2D,depth,0);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        if(status(0x8D40)!=0x8CD5)throw std::runtime_error("Shadow framebuffer incomplete");
        bind(0x8D40,0);glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);
        depthLocation=pGetUniformLocation(program,"renderDepth");matrixLocation=pGetUniformLocation(program,"shadowFromEye");enabledLocation=pGetUniformLocation(program,"shadowsEnabled");kindLocation=pGetUniformLocation(program,"materialKind");textureLocation=pGetUniformLocation(program,"useTexture");cloudLocation=pGetUniformLocation(program,"cloudParams");rightLocation=pGetUniformLocation(program,"camRight");upLocation=pGetUniformLocation(program,"camUp");backLocation=pGetUniformLocation(program,"camBack");boatLocation=pGetUniformLocation(program,"boatPosition");footLocation=pGetUniformLocation(program,"footPosition");goddessVelocityLocation=pGetUniformLocation(program,"goddessVelocity");boatVelocityLocation=pGetUniformLocation(program,"boatVelocity");pUniform1i(pGetUniformLocation(program,"shadowMap"),2);
        cloudCameraLocation=pGetUniformLocation(program,"cloudCameraWorld");
        for(int i=0;i<16;++i)trailLocation[i]=pGetUniformLocation(program,("wakeTrail["+std::to_string(i)+"]").c_str());
        // Shadows follow the art-directed front-left key, not the background disc.
        glm::vec3 focus(-2.f,4.f,-26.f);
        glm::vec3 sunDirection=glm::normalize(glm::vec3(-.48f,.48f,.74f));
        lightView=glm::lookAt(focus+sunDirection*80.f,focus,glm::vec3(0,1,0));lightProjection=glm::ortho(-46.f,46.f,-34.f,34.f,.2f,160.f);
        std::cout<<"Shadow framebuffer COMPLETE "<<size<<"x"<<size<<"\n";
    }
    void begin(){
        active(0x84C2);glBindTexture(GL_TEXTURE_2D,0);active(0x84C0);bind(0x8D40,framebuffer);glViewport(0,0,size,size);
        glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(3.4f,9.f);
        pUniform1i(depthLocation,1);glMatrixMode(GL_PROJECTION);glLoadMatrixf(glm::value_ptr(lightProjection));
    }
    void end(int w,int h,const glm::mat4& camera,bool enabled){
        glDisable(GL_POLYGON_OFFSET_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);bind(0x8D40,0);glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);glViewport(0,0,w,h);pUniform1i(depthLocation,0);pUniform1i(enabledLocation,enabled?1:0);
        auto bias=glm::scale(glm::translate(glm::mat4(1),glm::vec3(.5f)),glm::vec3(.5f));auto transform=bias*lightProjection*lightView*glm::inverse(camera);matrix(matrixLocation,1,GL_FALSE,glm::value_ptr(transform));
        active(0x84C2);glBindTexture(GL_TEXTURE_2D,depth);active(0x84C0);
        // Rows of the view matrix are the camera axes. Clouds sample the world view ray, not the screen.
        glm::vec3 right(camera[0][0],camera[1][0],camera[2][0]);
        glm::vec3 upAxis(camera[0][1],camera[1][1],camera[2][1]);
        glm::vec3 back(camera[0][2],camera[1][2],camera[2][2]);
        pUniform3f(rightLocation,right.x,right.y,right.z);
        pUniform3f(upLocation,upAxis.x,upAxis.y,upAxis.z);
        pUniform3f(backLocation,back.x,back.y,back.z);
        glm::vec3 cloudEye=glm::vec3(glm::inverse(camera)[3]);
        pUniform3f(cloudCameraLocation,cloudEye.x,cloudEye.y,cloudEye.z);
        // main.cpp writes sceneTime as 0 after this pass. Keep the frame clock in the unused x so water can move.
        pUniform3f(cloudLocation,float(glfwGetTime()),float(w),float(h));
        glm::vec3 goddess=scene_layout::luoshenPosition;
        glm::vec3 boat=scene_layout::boatPosition;
        float goddessYaw=scene_layout::luoshenYaw,goddessScale=scene_layout::luoshenScale;
        float boatYaw=scene_layout::boatYaw,boatScale=scene_layout::boatScale;
        for(const auto& item:layoutEditor.items){
            if(item.id=="luoshen"){goddess=item.pose.position;goddessYaw=item.pose.yaw;goddessScale=item.pose.scale;}
            if(item.id=="boat"){boat=item.pose.position;boatYaw=item.pose.yaw;boatScale=item.pose.scale;}
        }
        auto spun=[&](glm::vec2 local,float yaw){float r=glm::radians(yaw),c=std::cos(r),s=std::sin(r);return glm::vec2(local.x*c+local.y*s,-local.x*s+local.y*c);};
        // Lowest vertices of each mesh, relative to the pivot drawModel subtracts.
        glm::vec2 footShift=spun(glm::vec2(.348f,-.064f)*goddessScale,goddessYaw);
        glm::vec2 keelShift=spun(glm::vec2(.031f,-.050f)*boatScale,boatYaw);
        glm::vec3 foot(goddess.x+footShift.x,goddess.y,goddess.z+footShift.y);
        glm::vec3 keel(boat.x+keelShift.x,boat.y,boat.z+keelShift.y);
        static glm::vec3 prevGoddess,prevBoat;
        static double prevClock=-1.0;
        static bool havePrev=false;
        double now=glfwGetTime();
        glm::vec3 goddessVelocity(0.f),boatVelocity(0.f);
        float dt=havePrev?float(now-prevClock):0.f;
        if(havePrev&&dt>1e-4f&&dt<.25f){
            goddessVelocity=(goddess-prevGoddess)/dt;
            boatVelocity=(boat-prevBoat)/dt;
            goddessVelocity.y=0.f;boatVelocity.y=0.f;
            auto limit=[](glm::vec3 v){float len=glm::length(v);return len>1.6f?v*(1.6f/len):v;};
            goddessVelocity=limit(goddessVelocity);boatVelocity=limit(boatVelocity);
        }
        prevGoddess=goddess;prevBoat=boat;prevClock=now;havePrev=true;
        auto ashore=[&](glm::vec3 p){
            float reach=glm::smoothstep(-45.f,-36.f,p.z)*(1.f-glm::smoothstep(-9.f,-6.f,p.z));
            float shore=-3.5f+.65f*std::sin(p.z*.16f)+.35f*std::sin(p.z*.53f+.8f)+.12f*std::sin(p.z*1.4f)
                +reach*(.38f+.28f*std::sin(p.z*.41f))-.90f*std::exp(-std::pow((p.z+9.8f)/2.1f,2.f));
            return p.y>.12f||p.x<shore+.15f;
        };
        if(ashore(foot))goddessVelocity=glm::vec3(0.f);
        if(ashore(keel))boatVelocity=glm::vec3(0.f);
        // Independent world-fixed histories for the character and boat.
        static glm::vec3 trail[16];
        static double trailTime[16]={};
        static int nextTrail[2]={0,0};
        static double lastTrail[2]={-10.0,-10.0};
        static glm::vec3 lastContact[2]={foot,keel};
        glm::vec3 contact[2]={foot,keel};
        for(int emitter=0;emitter<2;++emitter){
            float moved=glm::length(contact[emitter]-lastContact[emitter]);
            bool teleport=moved>2.0f;
            if(teleport)for(int i=0;i<8;++i)trailTime[emitter*8+i]=0.0;
            bool validTime=dt>1e-4f&&dt<1.0f;
            // The hull drifts slowly, so it raises fewer rings but starts them at a
            // lower speed than the character does.
            const float gate=emitter==1?.020f:.06f;
            const double spacing=emitter==1?.80:.14;
            if(!teleport&&validTime&&!ashore(contact[emitter])&&moved/dt>gate&&now-lastTrail[emitter]>spacing){
                int slot=emitter*8+nextTrail[emitter];
                trail[slot]=contact[emitter];trailTime[slot]=now;
                nextTrail[emitter]=(nextTrail[emitter]+1)%8;lastTrail[emitter]=now;
                if(emitter==1)++boatWakeEmits;
            }
            lastContact[emitter]=contact[emitter];
        }
        for(int i=0;i<16;++i){
            float age=trailTime[i]>0.0?float(now-trailTime[i]):10.f;
            pUniform3f(trailLocation[i],trail[i].x,trail[i].z,age);
        }
        pUniform3f(footLocation,foot.x,foot.y,foot.z);
        pUniform3f(boatLocation,keel.x,keel.y,keel.z);
        pUniform3f(goddessVelocityLocation,goddessVelocity.x,goddessVelocity.y,goddessVelocity.z);
        pUniform3f(boatVelocityLocation,boatVelocity.x,boatVelocity.y,boatVelocity.z);
        pUniform1i(textureLocation,0);pUniform1i(kindLocation,30);
        glPushAttrib(GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
        glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glNormal3f(0.f,0.f,1.f);glBegin(GL_QUADS);
        glTexCoord2f(0.f,0.f);glVertex2f(-1.f,-1.f);glTexCoord2f(1.f,0.f);glVertex2f(1.f,-1.f);
        glTexCoord2f(1.f,1.f);glVertex2f(1.f,1.f);glTexCoord2f(0.f,1.f);glVertex2f(-1.f,1.f);glEnd();
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
        glPopAttrib();pUniform1i(kindLocation,0);
    }
    void destroy(){glDeleteTextures(1,&depth);remove(1,&framebuffer);}
};
