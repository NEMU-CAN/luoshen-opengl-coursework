#pragma once
#include <glm/glm.hpp>
#include <algorithm>

// A one-way farewell: arrive at the end of the curve, then remain there.
inline glm::vec3 farewellPosition(double seconds) {
    float t=static_cast<float>(std::clamp(seconds/45.0,0.0,1.0));
    // Smooth time at departure and arrival without changing the spatial curve.
    t=t*t*(3-2*t);
    const glm::vec3 p0(1,.045f,-1.2f),p1(1.2f,.045f,-2.2f),
                    p2(2.8f,.045f,-3.7f),p3(4.5f,.045f,-4);
    float s=1-t;
    return s*s*s*p0+3*s*s*t*p1+3*s*t*t*p2+t*t*t*p3;
}
