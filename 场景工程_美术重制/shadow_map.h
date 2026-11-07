#pragma once
// Depth-map shadows; colour and normal textures keep units 0 and 1.
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
    GLint depthLocation=-1,matrixLocation=-1,enabledLocation=-1;
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
        depthLocation=pGetUniformLocation(program,"renderDepth");matrixLocation=pGetUniformLocation(program,"shadowFromEye");enabledLocation=pGetUniformLocation(program,"shadowsEnabled");pUniform1i(pGetUniformLocation(program,"shadowMap"),2);
        // Same sun as the shader: world (-12, 5.5, 9), aimed at the folded bank and the two figures.
        lightView=glm::lookAt(glm::vec3(-12.f,5.5f,9.f),glm::vec3(-4.2f,.5f,-2.8f),glm::vec3(0,1,0));lightProjection=glm::ortho(-13.f,13.f,-13.f,13.f,.2f,64.f);
        std::cout<<"Shadow framebuffer COMPLETE "<<size<<"x"<<size<<"\n";
    }
    void begin(){
        active(0x84C2);glBindTexture(GL_TEXTURE_2D,0);active(0x84C0);bind(0x8D40,framebuffer);glViewport(0,0,size,size);
        glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(3.f,8.f);
        pUniform1i(depthLocation,1);glMatrixMode(GL_PROJECTION);glLoadMatrixf(glm::value_ptr(lightProjection));
    }
    void end(int w,int h,const glm::mat4& camera,bool enabled){
        glDisable(GL_POLYGON_OFFSET_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);bind(0x8D40,0);glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);glViewport(0,0,w,h);pUniform1i(depthLocation,0);pUniform1i(enabledLocation,enabled?1:0);
        auto bias=glm::scale(glm::translate(glm::mat4(1),glm::vec3(.5f)),glm::vec3(.5f));auto transform=bias*lightProjection*lightView*glm::inverse(camera);matrix(matrixLocation,1,GL_FALSE,glm::value_ptr(transform));
        active(0x84C2);glBindTexture(GL_TEXTURE_2D,depth);active(0x84C0);
    }
    void destroy(){glDeleteTextures(1,&depth);remove(1,&framebuffer);}
};
