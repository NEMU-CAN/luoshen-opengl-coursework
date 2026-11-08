#pragma once
// Rasterize native Chinese text once; render the resulting atlas over the scene.
inline GLuint makeSceneOverlay(bool help=false){
    const int w=1440,h=900;BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc||!bitmap||!bits)throw std::runtime_error("Text atlas allocation failed");
    auto previous=SelectObject(dc,bitmap);std::memset(bits,0,size_t(w)*h*4);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    auto text=[&](const wchar_t* value,int size,int x,int y,int width,const wchar_t* face=L"Microsoft YaHei"){
        HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
        auto old=SelectObject(dc,font);RECT rect{x,y,x+width,y+size+16};DrawTextW(dc,value,-1,&rect,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(font);
    };
    const wchar_t* title=L"洛水别绪";
    for(int i=0;i<4;++i){wchar_t glyph[]={title[i],0};text(glyph,38,1320,66+i*43,60,L"KaiTi");}
    const wchar_t* poem=L"怅盘桓而不能去";
    for(int i=0;i<8;++i){wchar_t glyph[]={poem[i],0};text(glyph,18,1287,73+i*23,40,L"KaiTi");}
    if(help){
        text(L"左键旋转   ·   右键平移   ·   滚轮缩放",16,46,823,850);
        text(L"1 主景    2 全景    3 洛神    4 曹植       R 回到主镜头    Tab 摆放    H 操作提示    F9 截图",15,46,855,1320);
    }
    GdiFlush(); // Complete batched GDI writes before reading DIB pixels on the CPU.
    std::vector<unsigned char> rgba(size_t(w)*h*4);auto* source=static_cast<unsigned char*>(bits);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        auto i=(size_t(y)*w+x)*4,j=(size_t(h-1-y)*w+x)*4;auto alpha=std::max({source[i],source[i+1],source[i+2]});
        rgba[j]=y<400?36:232;rgba[j+1]=y<400?58:224;rgba[j+2]=y<400?57:204;rgba[j+3]=alpha;
    }
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    return texture;
}
inline void drawSceneOverlay(GLuint texture,bool help=false){
    pUseProgram(0);glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    if(help){glColor4f(.07f,.12f,.12f,.7f);glBegin(GL_QUADS);glVertex2f(-1,-1);glVertex2f(1,-1);glVertex2f(1,-.79f);glVertex2f(-1,-.79f);glEnd();}
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor4f(1,1,1,1);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);glEnd();
    glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);
}
