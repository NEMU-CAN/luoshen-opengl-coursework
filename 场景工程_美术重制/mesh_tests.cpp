#include "mesh_data.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "scene_motion.h"
#include "pibo_motion.h"
#include "scene_layout.h"
#define ENVIRONMENT_EFFECTS_MATH_ONLY
#include "environment_effects.h"

using scene_motion::farewell;
using scene_motion::farewellPosition;

void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        require(glm::length(farewellPosition(0)-glm::vec3(1,.045f,-1.2f))<1e-5f,"Bezier start");
        require(glm::length(farewellPosition(45)-glm::vec3(4.05f,.045f,-6.1f))<1e-5f,"Bezier end");
        require(glm::length(farewellPosition(90)-farewellPosition(45))<1e-5f,"farewell must hold endpoint");
        require(glm::length(farewellPosition(-1)-farewellPosition(0))<1e-5f,"negative time clamps");
        require(farewellPosition(22.5).x>1.5f && farewellPosition(22.5).x<4,"Bezier midpoint advances");
        require(glm::length(farewell(0).position-scene_layout::luoshenPosition)<1e-5f,"start aligns static stance");
        require(farewell(0).phase==0&&farewell(20).phase==1&&farewell(40).phase==2,"narrative phases");
        std::cout<<"PASS: farewell path endpoints, midpoint and clamping\n";
        // 时间连续性与转向连续性：0.01 秒步长内位置与角度都不得跳变。
        {
            glm::vec3 previous=scene_motion::farewell(0).position;
            float previousYaw=scene_motion::farewell(0).yaw,maximumStep=0,maximumYawStep=0;
            for(int i=1;i<=4500;++i){
                const auto pose=scene_motion::farewell(i*.01);
                maximumStep=std::max(maximumStep,glm::length(pose.position-previous));
                maximumYawStep=std::max(maximumYawStep,std::abs(pose.yaw-previousYaw));
                require(std::isfinite(pose.position.x)&&std::isfinite(pose.yaw),"finite farewell pose");
                previous=pose.position;previousYaw=pose.yaw;
            }
            require(maximumStep<.01f,"farewell position must stay continuous");
            require(maximumYawStep<.20f,"farewell yaw must stay continuous");
            std::cout<<"PASS: farewell position/yaw continuity ("<<maximumStep<<", "<<maximumYawStep<<")\n";
        }
        // 路线不穿岸：全程位于左岸线以外、右侧开阔水域以内，并与小舟保持距离。
        for(int i=0;i<=900;++i){
            const auto point=scene_motion::farewell(i*.05).position;
            require(point.x>=environment_effects::shoreline(point.z)+.55f,"farewell crossed the left bank");
            require(point.x<8.0f,"farewell left the open water");
            require(glm::length(glm::vec2(point.x-scene_layout::boatPosition.x,point.z-scene_layout::boatPosition.z))>1.2f,
                    "farewell passed through the boat");
            require(point.y>=.03f&&point.y<=.06f,"farewell must skim the water plane");
        }
        std::cout<<"PASS: farewell route keeps off the bank, boat and water plane\n";
        // 小舟微动：0 秒静止、幅度克制、随时间变化且可复现。
        {
            const auto rest=scene_motion::boatMotion(0);
            require(std::abs(rest.heave)<1e-6f&&std::abs(rest.roll)<1e-6f&&
                    std::abs(rest.pitch)<1e-6f&&std::abs(rest.yaw)<1e-6f,"boat starts at rest");
            float maximumHeave=0,maximumRoll=0,maximumPitch=0,maximumYaw=0;
            for(int i=0;i<=4500;++i){
                const auto motion=scene_motion::boatMotion(i*.01);
                maximumHeave=std::max(maximumHeave,std::abs(motion.heave));
                maximumRoll=std::max(maximumRoll,std::abs(motion.roll));
                maximumPitch=std::max(maximumPitch,std::abs(motion.pitch));
                maximumYaw=std::max(maximumYaw,std::abs(motion.yaw));
            }
            require(maximumHeave<=.025f&&maximumRoll<=1.1f&&maximumPitch<=.6f&&maximumYaw<=.5f,
                    "boat motion must stay subtle");
            require(std::abs(scene_motion::boatMotion(5).heave)>1e-4f,"boat must actually move");
            const auto repeatA=scene_motion::boatMotion(7),repeatB=scene_motion::boatMotion(7);
            require(repeatA.heave==repeatB.heave&&repeatA.roll==repeatB.roll&&
                    repeatA.pitch==repeatB.pitch&&repeatA.yaw==repeatB.yaw,"boat motion deterministic");
            std::cout<<"PASS: boat heave/roll/pitch/yaw stays subtle and reversible\n";
        }
        // 披帛：肩边固定、中段轻摆、末端摆幅更大且相位更晚。
        {
            require(scene_motion::windEnvelope(0)<1e-5f,"wind starts calm");
            require(scene_motion::windEnvelope(12)>.4f,"wind rises during departure");
            require(pibo_motion::ownsKind(8)&&pibo_motion::ownsKind(9)&&pibo_motion::ownsKind(26),
                    "pibo kinds must be owned");
            auto sample=[](int kind,float s,double seconds,float wind)->glm::vec3{
                if(kind>=26){
                    // kind 26-29 使用 V 坐标参数化。
                    return pibo_motion::displacement(kind,glm::vec3(0),glm::vec2(.5f,.88f-.75f*s),seconds,wind);
                }
                return pibo_motion::displacement(kind,glm::vec3(-.033f-1.10f*s,1.403f-.418f*s,0),glm::vec2(0),seconds,wind);
            };
            for(int kind:{8,9,26,27,28,29}){
                require(glm::length(sample(kind,0.f,9.0,1.f))<1e-6f,"pibo root must stay pinned");
            }
            auto travel=[&](int kind,float s)->float{
                float maximum=0;
                for(int i=0;i<600;++i)maximum=std::max(maximum,glm::length(sample(kind,s,i*.01,1.f)));
                return maximum;
            };
            for(int kind:{8,9,26}){
                const float middle=travel(kind,.55f),tip=travel(kind,1.f);
                require(tip>middle&&middle>0.001f,"pibo tip must swing wider than the middle");
            }
            // 末端峰值出现得比中段晚：相位滞后形成根到梢的传播。
            auto peakTime=[&](int kind,float s)->double{
                double best=0;float value=0;
                for(int i=0;i<2400;++i){float v=std::abs(sample(kind,s,i*.005,1.f).z);if(v>value){value=v;best=i*.005;}}
                return best;
            };
            require(peakTime(8,1.f)>peakTime(8,.5f)+.01,"ribbon tip phase must lag the middle");
            require(glm::length(sample(8,1.f,9.0,0.f))<1e-6f,"zero wind must rest the pibo");
            std::cout<<"PASS: pibo root pin, travelling wave and zero-wind rest\n";
        }
        const glm::vec3 goddess(1,.045f,-1.2f),boat(-2,.025f,-6.9f),blossom(-3.34f,.46f,-2.65f);
        int visible=0;
        for(double time:{0.0,9.0,23.0})for(int i=0;i<environment_effects::waterPetalCount;++i){
            const auto pose=environment_effects::waterPetalPose(i,time);
            if(!environment_effects::visibleWaterPetal(pose,goddess,boat))continue;
            require(pose.position.x>=environment_effects::shoreline(pose.position.z)+.32f,"water petal crossed the bank");
            require(!environment_effects::inCentralClearance(pose.position),"water petal entered central clearance");
            ++visible;
        }
        require(visible>=105,"too few water petals remain after avoidance");
        const auto waterStart=environment_effects::waterPetalPose(5,0.0);
        require(glm::length(waterStart.position-environment_effects::waterPetalPose(5,0.0).position)<1e-7f,"water petals must be deterministic");
        require(glm::length(waterStart.position-environment_effects::waterPetalPose(5,2.0).position)>.001f,"water petals must move with shared time");
        for(int i=0;i<environment_effects::fallingPetalCount;++i){
            const auto pose=environment_effects::fallingPetalPose(i,12.0,blossom);
            require(pose.position.y>=blossom.y+.11f && pose.position.y<=blossom.y+3.96f,"falling petal height range");
            const float downwind=pose.position.x-blossom.x;
            if(i<environment_effects::canopyPetalCount)
                require(std::abs(downwind)<1.8f,"canopy petals must stay near blossom tree");
            else if(i<environment_effects::canopyPetalCount+environment_effects::downwindPetalCount+environment_effects::farPetalCount)
                require(downwind>1.f && downwind<4.4f && pose.position.z<blossom.z-1.4f,"downwind petals must spread over water");
            else
                require(pose.position.z>blossom.z+1.1f && pose.position.y<blossom.y+1.6f,"foreground petals must stay low and near");
        }
        std::cout<<"PASS: deterministic water/falling petals, bank and narrative clearances\n";
        auto dir=std::filesystem::temp_directory_path()/"luoshen-obj-fixture-20261104";
        std::filesystem::create_directories(dir);
        std::ofstream(dir/"colors.png")<<"fixture";
        std::ofstream(dir/"alpha.png")<<"fixture";
        std::ofstream(dir/"test.mtl")<<"newmtl rock\nKd 0.4 0.5 0.6\nmap_Kd colors.png\nnewmtl leaf\nmap_Kd colors.png\nmap_d alpha.png\n";
        std::ofstream(dir/"test.obj")<<"mtllib test.mtl\nv 0 0 0\nv 2 0 0\nv 2 1 0\nv 0 1 0\nvt 0.1 0.2\nvt 0.9 0.2\nvt 0.9 0.8\nvt 0.1 0.8\nvn 0 0 1\nusemtl rock\nf 1/1/1 2/2/1 3/3/1\nusemtl leaf\nf -4/1/1 -2/3/1 -1/4/1\n";
        std::ofstream(dir/"test.obj",std::ios::app)<<"v 100 -100 100\n";
        auto m=readSceneObj(dir/"test.obj");
        require(m.minimum.y==0 && m.maximum.x==2,"unused vertices must not move the model anchor");
        require(m.triangles==2,"OBJ faces must be loaded");
        require(m.batches.size()==2,"usemtl must split batches");
        require(m.materials[m.batches[1].material].name=="leaf","second material must be leaf");
        require(glm::length(m.batches[1].vertices[2].position-glm::vec3(0,1,0))<1e-6f,"negative position index");
        require(glm::length(m.batches[0].vertices[0].uv-glm::vec2(.1f,.2f))<1e-6f,"authored UV must be preserved");
        require(glm::length(m.batches[0].vertices[0].normal-glm::vec3(0,0,1))<1e-6f,"authored normal must be preserved");
        require(m.materials[m.batches[1].material].alphaMap.filename()=="alpha.png","separate opacity map");
        bool missing=false;try{readSceneObj(dir/"absent.obj");}catch(const std::exception&){missing=true;}
        require(missing,"missing OBJ must report failure");
        std::ofstream(dir/"bad.obj")<<"v 0 0 0\nf 1 2 3\n";
        bool bad=false;try{readSceneObj(dir/"bad.obj");}catch(const std::exception&){bad=true;}
        require(bad,"out-of-range indices must report failure");
        std::cout<<"PASS: UV, normals, negative indices, materials, alpha and input errors\n";
        if(argc>1){
            std::filesystem::path root(argv[1]);
            size_t count=0,total=0;
            for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
                if(entry.path().extension()!=".obj")continue;
                auto model=readSceneObj(entry.path());require(model.triangles>0,"empty real model");
                for(const auto& mat:model.materials){
                    if(!mat.colorMap.empty())require(std::filesystem::is_regular_file(mat.colorMap),"missing color map");
                    if(!mat.alphaMap.empty())require(std::filesystem::is_regular_file(mat.alphaMap),"missing opacity map");
                }
                ++count;total+=model.triangles;
            }
            const size_t expected=argc>2?std::stoull(argv[2]):11;
            require(count==expected,"prepared OBJ asset count must match expectation");
            std::cout<<"PASS: "<<count<<" real assets; "<<total<<" triangles\n";
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
