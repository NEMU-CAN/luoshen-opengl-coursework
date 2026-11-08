#pragma once
#include <glm/glm.hpp>
#include <cmath>

namespace environment_effects {
inline constexpr int waterPetalCount=48;
inline constexpr int fallingPetalCount=14;

struct PetalPose {
    glm::vec3 position{0};
    float yaw=0;
    float scale=1;
    glm::vec2 tilt{0};
};

inline float noise(float n){return glm::fract(std::sin(n*127.1f+31.7f)*43758.5453f);}
inline float shoreline(float z){return -3.5f+.65f*std::sin(z*.16f)+.35f*std::sin(z*.53f+.8f)+.12f*std::sin(z*1.4f);}

inline PetalPose waterPetalPose(int index,double seconds){
    const float seed=static_cast<float>(index)+7.f;
    const float time=static_cast<float>(seconds);
    const bool nearBank=index<34;
    float z=(nearBank?-11.5f:-10.5f)+noise(seed+18.f)*(nearBank?13.5f:12.f);
    z+=.18f*std::sin(time*.10f+seed*.43f);
    float x=nearBank
        ?shoreline(z)+.42f+noise(seed+4.f)*1.45f
        :1.65f+noise(seed+4.f)*5.2f;
    x+=.10f*std::sin(time*.13f+seed);
    PetalPose pose;
    pose.position={x,.014f+.003f*std::sin(time*.72f+seed),z};
    pose.yaw=glm::degrees(noise(seed+6.f)*6.2831853f+time*(.025f+.018f*noise(seed+9.f)));
    pose.scale=.50f+.36f*noise(seed+40.f);
    pose.tilt={2.f+4.f*noise(seed+22.f),3.f*std::sin(time*.35f+seed)};
    return pose;
}

inline bool inCentralClearance(const glm::vec3& position){
    const glm::vec2 offset((position.x+.35f)/1.25f,(position.z+4.4f)/3.15f);
    return glm::dot(offset,offset)<1.f;
}

inline bool visibleWaterPetal(const PetalPose& pose,const glm::vec3& goddess,const glm::vec3& boat){
    const glm::vec2 p(pose.position.x,pose.position.z);
    const glm::vec2 goddessXZ(goddess.x,goddess.z),boatXZ(boat.x,boat.z);
    return pose.position.x>=shoreline(pose.position.z)+.32f
        && glm::length(p-goddessXZ)>=.78f
        && glm::length(p-boatXZ)>=1.05f
        && !inCentralClearance(pose.position);
}

inline PetalPose fallingPetalPose(int index,double seconds,const glm::vec3& blossom){
    const float seed=static_cast<float>(index)+91.f;
    const float time=static_cast<float>(seconds);
    const float duration=7.5f+3.5f*noise(seed+3.f);
    float elapsed=std::fmod(time+duration*noise(seed+8.f),duration);
    if(elapsed<0)elapsed+=duration;
    const float t=elapsed/duration;
    const float sway=std::sin(t*9.424778f+seed);
    PetalPose pose;
    pose.position=blossom+glm::vec3(
        -.35f+.75f*noise(seed+11.f)+.48f*t+.18f*sway,
        .20f+3.75f*(1.f-t),
        -.62f+1.24f*noise(seed+17.f)+.16f*std::sin(t*12.56637f+seed*.7f));
    pose.yaw=noise(seed+23.f)*360.f+time*16.f*(noise(seed+25.f)>.5f?1.f:-1.f);
    pose.scale=.56f+.30f*noise(seed+29.f);
    pose.tilt={28.f+46.f*noise(seed+31.f),24.f*std::sin(t*12.56637f+seed)};
    return pose;
}

#ifndef ENVIRONMENT_EFFECTS_MATH_ONLY
// Include this header after SceneModel and drawModel are defined.
inline void drawWaterPetals(double seconds,const glm::vec3& goddess,const glm::vec3& boat,
                            const glm::mat4& view,const SceneModel (&petals)[3]){
    for(int i=0;i<waterPetalCount;++i){
        const auto pose=waterPetalPose(i,seconds);
        if(!visibleWaterPetal(pose,goddess,boat))continue;
        drawModel(petals[i%3],view,pose.position,pose.yaw,pose.scale,glm::vec3(1),glm::vec3(1),pose.tilt);
    }
}

inline void drawFallingPetals(double seconds,const glm::vec3& blossom,const glm::mat4& view,
                              const SceneModel (&petals)[3]){
    for(int i=0;i<fallingPetalCount;++i){
        const auto pose=fallingPetalPose(i,seconds,blossom);
        drawModel(petals[(i+1)%3],view,pose.position,pose.yaw,pose.scale,glm::vec3(1),glm::vec3(1),pose.tilt);
    }
}
#endif
}
