#pragma once
#include "scene_layout.h"
#include "layout_editor.h"
// Depth-map shadows; colour and normal textures keep units 0 and 1.
extern LayoutEditor layoutEditor;
struct ShadowMap {
    static constexpr int size=2048;
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using Status=GLenum(APIENTRY*)(GLenum);
    using Matrix=void(APIENTRY*)(GLint,GLsizei,GLboolean,const GLfloat*);
    using Active=void(APIENTRY*)(GLenum);
    GLuint framebuffer=0,depth=0;Gen gen=nullptr,remove=nullptr;Bind bind=nullptr;Attach attach=nullptr;Status status=nullptr;Matrix matrix=nullptr;Active active=nullptr;
    glm::mat4 lightView,lightProjection;
    GLint depthLocation=-1,matrixLocation=-1,enabledLocation=-1,kindLocation=-1,textureLocation=-1,cloudLocation=-1,rightLocation=-1,upLocation=-1,backLocation=-1,boatLocation=-1,goddessVelocityLocation=-1,boatVelocityLocation=-1;
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
        depthLocation=pGetUniformLocation(program,"renderDepth");matrixLocation=pGetUniformLocation(program,"shadowFromEye");enabledLocation=pGetUniformLocation(program,"shadowsEnabled");kindLocation=pGetUniformLocation(program,"materialKind");textureLocation=pGetUniformLocation(program,"useTexture");cloudLocation=pGetUniformLocation(program,"cloudParams");rightLocation=pGetUniformLocation(program,"camRight");upLocation=pGetUniformLocation(program,"camUp");backLocation=pGetUniformLocation(program,"camBack");boatLocation=pGetUniformLocation(program,"boatPosition");goddessVelocityLocation=pGetUniformLocation(program,"goddessVelocity");boatVelocityLocation=pGetUniformLocation(program,"boatVelocity");pUniform1i(pGetUniformLocation(program,"shadowMap"),2);
        // Same sun as the shader: world (-12, 5.5, 9), aimed at the folded bank and the two figures.
        lightView=glm::lookAt(glm::vec3(-12.f,5.5f,9.f),glm::vec3(-4.2f,.5f,-2.8f),glm::vec3(0,1,0));lightProjection=glm::ortho(-13.f,13.f,-13.f,13.f,.2f,64.f);
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
        // main.cpp writes sceneTime as 0 after this pass. Keep the frame clock in the unused x so water can move.
        pUniform3f(cloudLocation,float(glfwGetTime()),float(w),float(h));
        glm::vec3 goddess=scene_layout::luoshenPosition;
        glm::vec3 boat=scene_layout::boatPosition;
        for(const auto& item:layoutEditor.items){
            if(item.id=="luoshen")goddess=item.pose.position;
            if(item.id=="boat")boat=item.pose.position;
        }
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
        pUniform3f(boatLocation,boat.x,boat.y,boat.z);
        pUniform3f(goddessVelocityLocation,goddessVelocity.x,goddessVelocity.y,goddessVelocity.z);
        pUniform3f(boatVelocityLocation,boatVelocity.x,boatVelocity.y,boatVelocity.z);
        pUniform1i(textureLocation,0);pUniform1i(kindLocation,8);
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
