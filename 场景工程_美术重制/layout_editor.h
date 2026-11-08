#pragma once

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>

class LayoutEditor {
public:
    struct Pose { glm::vec3 position; float yaw, scale; };
    struct Item {
        std::string id, name;
        Pose pose;
        glm::vec3 size;
        bool waterLocked;
        const std::vector<glm::vec3>* triangles=nullptr;
        glm::vec3 stretch{1};
    };
    std::vector<Item> items;
    std::filesystem::path path;
    bool active=false;

    void load(){
        std::ifstream in(path);
        std::string line;
        while(std::getline(in,line)){
            std::istringstream row(line);
            std::string id;
            Pose p;
            if(!(row>>id>>p.position.x>>p.position.y>>p.position.z>>p.yaw>>p.scale))continue;
            if(!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||!std::isfinite(p.position.z)||
               !std::isfinite(p.yaw)||!(p.scale>=.05f&&p.scale<=30.f))continue;
            for(auto& item:items)if(item.id==id){
                if(item.waterLocked)p.position.y=item.pose.position.y;
                item.pose=p;
                break;
            }
        }
    }

    void save(GLFWwindow* window){
        auto temporary=path;
        temporary+=L".tmp";
        std::ofstream out(temporary);
        out<<std::setprecision(9);
        for(const auto& item:items){
            const auto& p=item.pose;
            out<<item.id<<' '<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<p.yaw<<' '<<p.scale<<'\n';
        }
        out.close();
        bool saved=out&&MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
        title(window,saved?u8"  |  已保存":u8"  |  保存失败");
    }

    void title(GLFWwindow* window,const char* status="") const {
        if(!active){
            glfwSetWindowTitle(window,u8"洛水别绪  |  Tab 摆放  |  1/2/3/4 镜头  |  H 帮助");
            return;
        }
        std::ostringstream label;
        label<<u8"摆放  |  ";
        if(selected>=0){
            const auto& item=items[selected];
            const auto& p=item.pose;
            label<<item.name<<std::fixed<<std::setprecision(2)<<"  XYZ "<<p.position.x<<", "<<p.position.y<<", "<<p.position.z
                 <<u8"  角度 "<<p.yaw<<u8"  缩放 "<<p.scale;
        }else label<<u8"点击船、树或岩石";
        label<<u8"  |  箭头移动  紫圈旋转  黄块缩放  Shift 微调  Ctrl+Z 撤销  Ctrl+S 保存"<<status;
        glfwSetWindowTitle(window,label.str().c_str());
    }

    void endDrag(){dragAxis=-1;}

    bool key(GLFWwindow* window,int key,int action,int mods){
        if(!active||action==GLFW_RELEASE)return false;
        if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_S){
            if(action==GLFW_PRESS)save(window);
            return true;
        }
        if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_Z){
            if(action==GLFW_PRESS&&!history.empty()){
                endDrag();
                selected=history.back().index;
                items[selected].pose=history.back().pose;
                history.pop_back();
                title(window);
            }
            return true;
        }
        if(selected<0)return false;
        int direction=(key==GLFW_KEY_Q||key==GLFW_KEY_MINUS||key==GLFW_KEY_KP_SUBTRACT)?-1:1;
        float step=(mods&GLFW_MOD_SHIFT)?.2f:1.f;
        if(key==GLFW_KEY_Q||key==GLFW_KEY_E){
            remember();
            items[selected].pose.yaw+=5.f*step*direction;
        }else if(key==GLFW_KEY_EQUAL||key==GLFW_KEY_MINUS||key==GLFW_KEY_KP_ADD||key==GLFW_KEY_KP_SUBTRACT){
            remember();
            auto& scale=items[selected].pose.scale;
            scale=glm::clamp(scale*std::exp(.05f*step*direction),.05f,30.f);
        }else return false;
        title(window);
        return true;
    }

    void click(GLFWwindow* window,glm::vec2 cursor){
        endDrag();
        if(selected>=0){
            auto origin=project(anchor(items[selected]));
            if(origin)for(int control=3;control<5;++control){
                if(glm::length(cursor-controlCenter(control,*origin))<13.f){
                    remember();
                    dragAxis=control;
                    dragCursor=cursor;
                    return;
                }
            }
            if(origin)for(int axis=0;axis<3;++axis){
                auto tip=handle(axis,*origin);
                if(tip&&glm::length(cursor-*tip)<13.f){
                    auto unit=project(anchor(items[selected])+axisVector(axis));
                    if(!unit||glm::length(*unit-*origin)<1.f)continue;
                    remember();
                    dragAxis=axis;
                    dragVector=*unit-*origin;
                    dragCursor=cursor;
                    return;
                }
            }
        }
        selected=-1;
        if(width<1||height<1)return;
        auto inverse=glm::inverse(camera);
        auto ndc=glm::vec2(2.f*cursor.x/width-1.f,1.f-2.f*cursor.y/height);
        auto nearPoint=inverse*glm::vec4(ndc,-1,1),farPoint=inverse*glm::vec4(ndc,1,1);
        glm::vec3 start=glm::vec3(nearPoint)/nearPoint.w;
        glm::vec3 direction=glm::normalize(glm::vec3(farPoint)/farPoint.w-start);
        float nearest=160.f;
        for(int i=0;i<int(items.size());++i){
            const auto& item=items[i];
            auto rotation=glm::rotate(glm::mat4(1),glm::radians(-item.pose.yaw),glm::vec3(0,1,0));
            auto localStart=glm::vec3(rotation*glm::vec4(start-item.pose.position,1));
            auto localDirection=glm::vec3(rotation*glm::vec4(direction,0));
            auto extent=item.size*item.pose.scale;
            float hit=intersect(localStart,localDirection,{-extent.x/2,0,-extent.z/2},{extent.x/2,extent.y,extent.z/2},nearest);
            if(hit<nearest&&item.triangles){
                auto scale=item.stretch*item.pose.scale;
                hit=hitMesh(*item.triangles,localStart/scale,localDirection/scale,nearest);
            }
            if(hit<nearest){nearest=hit;selected=i;}
        }
        title(window);
    }

    void drag(GLFWwindow* window,glm::vec2 cursor,bool fine){
        if(dragAxis<0)return;
        auto delta=cursor-dragCursor;
        auto& pose=items[selected].pose;
        float step=fine?.2f:1.f;
        if(dragAxis==3)pose.yaw+=delta.x*step;
        else if(dragAxis==4)pose.scale=glm::clamp(pose.scale*std::exp(delta.x*.012f*step),.05f,30.f);
        else pose.position[dragAxis]+=glm::dot(delta,dragVector)/glm::dot(dragVector,dragVector)*step;
        dragCursor=cursor;
        title(window);
    }

    void draw(const glm::mat4& viewProjection,int windowWidth,int windowHeight){
        camera=viewProjection;
        width=windowWidth;
        height=windowHeight;
        if(!active||selected<0||width<1||height<1)return;
        auto origin=project(anchor(items[selected]));
        if(!origin)return;
        // The scene overlay leaves both matrices at identity.
        auto vertex=[&](glm::vec2 p){glVertex2f(2*p.x/width-1,1-2*p.y/height);};
        glDisable(GL_DEPTH_TEST);
        for(int axis=0;axis<3;++axis){
            auto tip=handle(axis,*origin);
            if(!tip)continue;
            auto direction=glm::normalize(*tip-*origin);
            glm::vec2 side(-direction.y,direction.x);
            constexpr float colors[3][3]={{1,.3f,.24f},{.35f,1,.45f},{.30f,.66f,1}};
            glColor3fv(colors[axis]);
            glLineWidth(3);
            glBegin(GL_LINES);vertex(*origin);vertex(*tip);glEnd();
            glBegin(GL_TRIANGLES);vertex(*tip);vertex(*tip-direction*13.f+side*6.f);vertex(*tip-direction*13.f-side*6.f);glEnd();
        }
        auto rotation=controlCenter(3,*origin);
        glColor3f(.88f,.55f,1.f);
        glBegin(GL_LINE_LOOP);
        for(int i=0;i<24;++i){
            float a=i*6.2831853f/24;
            vertex(rotation+9.f*glm::vec2(std::cos(a),std::sin(a)));
        }
        glEnd();
        auto scale=controlCenter(4,*origin);
        glColor3f(1.f,.85f,.35f);
        glBegin(GL_QUADS);
        vertex(scale+glm::vec2(-7,-7));vertex(scale+glm::vec2(7,-7));
        vertex(scale+glm::vec2(7,7));vertex(scale+glm::vec2(-7,7));
        glEnd();
        glLineWidth(1);
        glEnable(GL_DEPTH_TEST);
    }

private:
    struct Undo {int index;Pose pose;};
    std::vector<Undo> history;
    int selected=-1,dragAxis=-1,width=0,height=0;
    glm::mat4 camera{1};
    glm::vec2 dragVector{},dragCursor{};

    void remember(){history.push_back({selected,items[selected].pose});}
    static glm::vec3 anchor(const Item& item){
        return item.pose.position+glm::vec3(0,item.size.y*item.pose.scale*.45f,0);
    }
    static glm::vec3 axisVector(int axis){glm::vec3 v(0);v[axis]=1;return v;}
    static glm::vec2 controlCenter(int control,glm::vec2 origin){
        return origin+glm::vec2(control==3?-28.f:28.f,36.f);
    }
    std::optional<glm::vec2> project(glm::vec3 point) const {
        auto clip=camera*glm::vec4(point,1);
        if(clip.w<=.01f)return {};
        return glm::vec2((clip.x/clip.w+1)*width/2,(1-clip.y/clip.w)*height/2);
    }
    std::optional<glm::vec2> handle(int axis,glm::vec2 origin) const {
        const auto& item=items[selected];
        if(axis==1&&item.waterLocked)return {};
        auto end=project(anchor(item)+axisVector(axis));
        if(!end||glm::length(*end-origin)<1.f)return {};
        return origin+glm::normalize(*end-origin)*64.f;
    }
    static float intersect(glm::vec3 origin,glm::vec3 direction,glm::vec3 minimum,glm::vec3 maximum,float limit){
        float enter=0,leave=limit;
        for(int axis=0;axis<3;++axis){
            if(std::abs(direction[axis])<1e-6f){
                if(origin[axis]<minimum[axis]||origin[axis]>maximum[axis])return limit;
                continue;
            }
            float a=(minimum[axis]-origin[axis])/direction[axis],b=(maximum[axis]-origin[axis])/direction[axis];
            enter=std::max(enter,std::min(a,b));
            leave=std::min(leave,std::max(a,b));
            if(enter>leave)return limit;
        }
        return enter;
    }
    static float hitMesh(const std::vector<glm::vec3>& vertices,glm::vec3 origin,glm::vec3 direction,float nearest){
        for(size_t i=0;i+2<vertices.size();i+=3){
            auto a=vertices[i],edge1=vertices[i+1]-a,edge2=vertices[i+2]-a;
            auto p=glm::cross(direction,edge2);
            float determinant=glm::dot(edge1,p);
            if(std::abs(determinant)<1e-7f)continue;
            auto t=origin-a;
            float u=glm::dot(t,p)/determinant;
            if(u<0||u>1)continue;
            auto q=glm::cross(t,edge1);
            float v=glm::dot(direction,q)/determinant;
            if(v<0||u+v>1)continue;
            float distance=glm::dot(edge2,q)/determinant;
            if(distance>=0&&distance<nearest)nearest=distance;
        }
        return nearest;
    }
};
