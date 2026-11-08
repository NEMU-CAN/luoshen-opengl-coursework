#include "mesh_data.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "scene_motion.h"
#define ENVIRONMENT_EFFECTS_MATH_ONLY
#include "environment_effects.h"

void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        require(glm::length(farewellPosition(0)-glm::vec3(1,.045f,-1.2f))<1e-5f,"Bezier start");
        require(glm::length(farewellPosition(45)-glm::vec3(4.5f,.045f,-4))<1e-5f,"Bezier end");
        require(glm::length(farewellPosition(90)-farewellPosition(45))<1e-5f,"farewell must hold endpoint");
        require(glm::length(farewellPosition(-1)-farewellPosition(0))<1e-5f,"negative time clamps");
        require(farewellPosition(22.5).x>1.5f && farewellPosition(22.5).x<4,"Bezier midpoint advances");
        std::cout<<"PASS: farewell path endpoints, midpoint and clamping\n";
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
            require(pose.position.y>=blossom.y+.19f && pose.position.y<=blossom.y+3.96f,"falling petal height range");
            require(std::abs(pose.position.x-blossom.x)<1.8f,"falling petals must stay near blossom tree");
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
