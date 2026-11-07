#pragma once
#include <glm/glm.hpp>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <limits>

struct SceneVertex { glm::vec3 position, normal; glm::vec2 uv; };
struct SceneMaterial {
    std::string name;
    glm::vec3 diffuse{1.0f};
    std::filesystem::path colorMap, alphaMap;
};
struct MeshBatch { size_t material=0; std::vector<SceneVertex> vertices; };
struct MeshData {
    std::vector<SceneMaterial> materials;
    std::vector<MeshBatch> batches;
    glm::vec3 minimum{0}, maximum{0};
    size_t triangles=0;
};
inline std::string meshTrim(std::string s) {
    const auto first=s.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return {};
    return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
inline void readSceneMtl(const std::filesystem::path& path,MeshData& model,
                         std::unordered_map<std::string,size_t>& ids) {
    std::ifstream in(path);if(!in)throw std::runtime_error("Cannot read material: "+path.u8string());
    SceneMaterial* current=nullptr;std::string line;
    while(std::getline(in,line)) {
        std::istringstream s(line);std::string tag;s>>tag;
        if(tag=="newmtl") {
            std::string name;std::getline(s,name);name=meshTrim(name);
            auto it=ids.find(name);
            if(it==ids.end()){ids[name]=model.materials.size();model.materials.push_back({});current=&model.materials.back();}
            else current=&model.materials[it->second];
            current->name=name;
        } else if(current && tag=="Kd")s>>current->diffuse.x>>current->diffuse.y>>current->diffuse.z;
        else if(current && (tag=="map_Kd"||tag=="map_d")) {
            std::string relative;std::getline(s,relative);relative=meshTrim(relative);
            if(relative.empty()||relative[0]=='-')throw std::runtime_error("Unsupported color/alpha map options");
            auto texture=(path.parent_path()/std::filesystem::u8path(relative)).lexically_normal();
            if(!std::filesystem::is_regular_file(texture))throw std::runtime_error("Missing material image: "+texture.u8string());
            if(tag=="map_Kd")current->colorMap=texture;else current->alphaMap=texture;
        }
    }
}
inline MeshData readSceneObj(const std::filesystem::path& path) {
    std::ifstream in(path);if(!in)throw std::runtime_error("Cannot read OBJ: "+path.u8string());
    MeshData result;result.materials.push_back({"default"});
    std::unordered_map<std::string,size_t> ids{{"default",0}};
    std::vector<glm::vec3> positions,normals;std::vector<glm::vec2> uvs;
    std::unordered_map<size_t,size_t> batchIds;
    size_t active=0,lineNo=0;std::string line;
    result.minimum=glm::vec3(std::numeric_limits<float>::max());result.maximum=-result.minimum;
    auto index=[](const std::string& token,size_t count)->size_t {
        size_t used=0;int raw=std::stoi(token,&used);
        if(used!=token.size()||raw==0)throw std::runtime_error("Invalid OBJ index");
        long long n=raw>0?raw-1LL:static_cast<long long>(count)+raw;
        if(n<0||n>=static_cast<long long>(count))throw std::runtime_error("OBJ index out of range");
        return static_cast<size_t>(n);
    };
    try {
        while(std::getline(in,line)) {
            ++lineNo;std::istringstream s(line);std::string tag;s>>tag;
            if(tag=="v") {
                glm::vec3 v;if(!(s>>v.x>>v.y>>v.z)||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::runtime_error("Invalid position");
                positions.push_back(v);
            } else if(tag=="vt") {
                glm::vec2 v;if(!(s>>v.x>>v.y))throw std::runtime_error("Invalid UV");uvs.push_back(v);
            } else if(tag=="vn") {
                glm::vec3 v;if(!(s>>v.x>>v.y>>v.z)||glm::length(v)<1e-9f)throw std::runtime_error("Invalid normal");normals.push_back(glm::normalize(v));
            } else if(tag=="mtllib") {
                std::string relative;std::getline(s,relative);readSceneMtl(path.parent_path()/std::filesystem::u8path(meshTrim(relative)),result,ids);
            } else if(tag=="usemtl") {
                std::string name;std::getline(s,name);name=meshTrim(name);
                if(!ids.count(name))throw std::runtime_error("Undefined material: "+name);
                active=ids.at(name);
            } else if(tag=="f") {
                std::vector<SceneVertex> face;std::string token;
                while(s>>token) {
                    if(token[0]=='#')break;
                    std::vector<std::string> parts;size_t start=0;
                    for(size_t p=0;p<=token.size();++p)if(p==token.size()||token[p]=='/'){parts.push_back(token.substr(start,p-start));start=p+1;}
                    if(parts.empty()||parts.size()>3)throw std::runtime_error("Invalid face token");
                    SceneVertex v{};v.position=positions.at(index(parts[0],positions.size()));
                    if(parts.size()>1&&!parts[1].empty())v.uv=uvs.at(index(parts[1],uvs.size()));
                    if(parts.size()>2&&!parts[2].empty())v.normal=normals.at(index(parts[2],normals.size()));
                    face.push_back(v);
                }
                if(face.size()<3)throw std::runtime_error("Face requires three vertices");
                if(!batchIds.count(active)){batchIds[active]=result.batches.size();result.batches.push_back({active,{}});}
                auto& vertices=result.batches[batchIds.at(active)].vertices;
                for(size_t i=1;i+1<face.size();++i) {
                    SceneVertex tri[]={face[0],face[i],face[i+1]};
                    auto n=glm::cross(tri[1].position-tri[0].position,tri[2].position-tri[0].position);
                    if(glm::length(n)<1e-12f)continue;
                    n=glm::normalize(n);
                    for(auto& v:tri){if(glm::length(v.normal)<1e-9f)v.normal=n;vertices.push_back(v);result.minimum=glm::min(result.minimum,v.position);result.maximum=glm::max(result.maximum,v.position);}
                    ++result.triangles;
                }
            }
        }
        if(!result.triangles)throw std::runtime_error("No drawable triangles");
    } catch(const std::exception& e){throw std::runtime_error(path.u8string()+":"+std::to_string(lineNo)+": "+e.what());}
    return result;
}
