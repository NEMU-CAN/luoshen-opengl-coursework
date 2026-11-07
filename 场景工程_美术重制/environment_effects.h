#pragma once
#include <glm/glm.hpp>
#include <cmath>

// Independent environment module. Include after main.cpp's material() helper.
// It consumes the shared clock, so Space and R affect petals with everything else.
namespace environment_effects {
inline float noise(float n){return glm::fract(std::sin(n*127.1f+31.7f)*43758.5453f);}
inline void drawWaterPetals(double seconds,const glm::vec3& goddess){
    const float time=static_cast<float>(seconds);
    for(int i=0;i<72;++i){
        float seed=static_cast<float>(i)+7.f;
        // More petals beneath the blossom, fewer across the open central water.
        float x=i<38?-2.8f+noise(seed)*3.5f:-2.4f+noise(seed)*8.2f;
        float z=i<38?-3.0f+noise(seed+18.f)*6.7f:-8.5f+noise(seed+18.f)*11.8f;
        x+=.14f*std::sin(time*.12f+seed);
        z+=.22f*std::sin(time*.08f+seed*.43f);
        if(x<bank(z)+.38f)continue;
        if(glm::length(glm::vec2(x-goddess.x,z-goddess.z))<.62f)continue;
        if(i>=38 && std::abs(x+.4f)<.70f && z<-3.f)continue;
        float angle=noise(seed+6.f)*6.2831853f+time*.045f;
        float size=.028f+.027f*noise(seed+40.f);
        glm::vec3 center(x,.014f+.003f*std::sin(time*.7f+seed),z);
        glm::vec3 along(std::cos(angle)*size,0,std::sin(angle)*size);
        glm::vec3 across(-std::sin(angle)*size*.48f,0,std::cos(angle)*size*.48f);
        glm::vec3 points[]={center-along*.70f,center+across,center+along,center-across};
        glm::vec3 fold=center+glm::vec3(0,.004f,0);
        material(i%4?glm::vec3(.94f,.82f,.67f):glm::vec3(.86f,.64f,.57f));
        glNormal3f(0,1,0);glBegin(GL_TRIANGLES);
        for(int j=0;j<4;++j){
            for(auto p:{fold,points[j],points[(j+1)%4]})glVertex3f(p.x,p.y,p.z);
        }
        glEnd();
    }
}
}
