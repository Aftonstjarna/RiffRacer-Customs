#pragma once
#include "paint.hpp"
#include "car_hud_preview.hpp"
#include <cairo.h>
namespace livery {
struct PreviewProjection {
    double scale=0,cx=0,cy=0,yaw=0,pitch=0;
    std::array<double,2> project(V v) const {
        double x=std::cos(yaw)*v.x+std::sin(yaw)*v.z;
        double z=-std::sin(yaw)*v.x+std::cos(yaw)*v.z;
        double y=std::cos(pitch)*v.y-std::sin(pitch)*z;
        return {cx+x*scale,cy-y*scale};
    }
    V screenDelta(double x,double y) const {
        return {F((std::cos(yaw)*x-std::sin(pitch)*std::sin(yaw)*y)/scale),
                F(-std::cos(pitch)*y/scale),
                F((std::sin(yaw)*x+std::sin(pitch)*std::cos(yaw)*y)/scale)};
    }
};
struct PreviewHit {
    int width=0,height=0;double scale=1,offsetX=0,offsetY=0;std::vector<double> depth;
    V point(const PreviewProjection &projection,double x,double y)const{
        int ix=int(std::floor((x-offsetX)/scale)),iy=int(std::floor((y-offsetY)/scale));double z=0;
        if(ix>=0&&iy>=0&&ix<width&&iy<height&&!depth.empty()){double sample=depth.at(size_t(iy)*width+ix);if(sample>-1e29)z=sample;}
        auto v=projection.screenDelta(x-projection.cx,y-projection.cy);
        return {F(v.x-std::sin(projection.yaw)*std::cos(projection.pitch)*z),F(v.y+std::sin(projection.pitch)*z),F(v.z+std::cos(projection.yaw)*std::cos(projection.pitch)*z)};
    }
};
inline PreviewProjection cameraProjection(const Model &model,int width,int height,double yaw,double pitch,double zoom,double panX,double panY){
    double minx=1e30,maxx=-1e30,miny=1e30,maxy=-1e30;
    for(const auto &node:model.nodes)if(bodyNode(node))for(int index:node.meshes)for(auto vertex:model.meshes.at(index).positions){auto v=transform(node.matrix,vertex,true);double x=F(std::cos(yaw)*v.x+std::sin(yaw)*v.z),z=-std::sin(yaw)*v.x+std::cos(yaw)*v.z,y=F(std::cos(pitch)*v.y-std::sin(pitch)*z);minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);}
    double reduction=std::min({1.,1000./width,800./height});int w=std::max(1,int(std::lround(width*reduction))),h=std::max(1,int(std::lround(height*reduction)));double display=std::min(double(width)/w,double(height)/h);
    double scale=std::min(w*.82/std::max(.01,maxx-minx),h*.78/std::max(.01,maxy-miny))*zoom*display;
    return {scale,width*.5+panX-(minx+maxx)*.5*scale,height*.5+panY+(miny+maxy)*.5*scale,yaw,pitch};
}
// Native EADB geometry/node transforms, with an approximate studio-light paint
// ramp. The original D3D shader and environment reflections are not reproduced.
inline void drawCar(cairo_t *cr,const Model &model,const PaintConfig &paint,
                    const PaintSelection &selection,int width,int height,double yaw,double zoom,const Image *decal=nullptr,const Model *attachments=nullptr,int selectedNode=-1,double pitch=.30,PreviewProjection *projection=nullptr,double panX=0,double panY=0,PreviewHit *hit=nullptr,const CarHudPreview *hudPreview=nullptr,const Model *hudNodes=nullptr,const std::map<std::string,std::array<double,3>> *materialColors=nullptr) {
    if(width<=0 || height<=0)return;
    // Bound raster cost using one scale for both axes; independent caps distort
    // the model when a wide or tall window stretches the raster back to fit.
    const double reduction=std::min({1.,1000./width,800./height});
    const int w=std::max(1,int(std::lround(width*reduction))),h=std::max(1,int(std::lround(height*reduction)));
    const double displayScale=std::min(double(width)/w,double(height)/h);
    auto displayTransform=[&]{cairo_translate(cr,(width-w*displayScale)*.5,(height-h*displayScale)*.5);cairo_scale(cr,displayScale,displayScale);};
    struct Point {double x,y,z;std::array<double,3> color;double u,v,light;bool decal;};
    std::vector<Point> points;std::vector<std::array<size_t,3>> faces;
    auto rgb=[&](const char *key){auto v=paint.get(selection,key,3);std::array<double,3> c{.35,.4,.5};if(v.size()==3)for(int k=0;k<3;k++)c[k]=v[k]/255.;return c;};
    auto c0=rgb("col0"),c1=rgb("col1"),c2=rgb("col2"),spec=rgb("specCol"),a0=rgb("alloysCol0"),a1=rgb("alloysCol1");
    double sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    auto rotate=[&](V p){double x=cy*p.x+sy*p.z,z=-sy*p.x+cy*p.z;return V{F(x),F(cp*p.y-sp*z),F(sp*p.y+cp*z)};};
    for(const auto &node:model.nodes)if(bodyNode(node))for(int index:node.meshes) {
        const auto &mesh=model.meshes.at(index);const auto &mat=model.materials.at(mesh.material);
        std::string name=lower(mat.at("name").get<std::string>());
        bool body=name.find("carpaint")!=std::string::npos;
        bool wheel=name.find("wheel")!=std::string::npos || name.find("alloy")!=std::string::npos;
        auto base=material(mat)["pbrMetallicRoughness"]["baseColorFactor"].get<std::array<float,4>>();
        size_t first=points.size();
        for(size_t i=0;i<mesh.positions.size();i++) {
            auto p=rotate(transform(node.matrix,mesh.positions[i],true));
            auto n=rotate(transform(node.matrix,mesh.normals[i]));double len=std::max(1e-9,length(n));
            double light=std::clamp((n.x*-.35+n.y*.75+n.z*.55)/len,0.,1.);
            std::array<double,3> color{};
            for(int k=0;k<3;k++) {
                if(body){double t=std::clamp(light*1.65,0.,1.65);color[k]=t<1?c0[k]*(1-t)+c1[k]*t:c1[k]*(2-t)+c2[k]*(t-1);color[k]+=spec[k]*std::pow(light,28)*.22;}
                else if(wheel)color[k]=(a0[k]*(1-light)+a1[k]*light)*(.4+.6*light);
                else {std::array<double,3> c{base[0],base[1],base[2]};if(materialColors)for(auto &entry:*materialColors)if(name.rfind(entry.first,0)==0){c=entry.second;break;}color[k]=c[k]*(.35+.65*light);}
                color[k]=std::clamp(color[k],0.,1.);
            }
            points.push_back({p.x,p.y,p.z,color,mesh.uv.at(i)[0],mesh.uv.at(i)[1],.4+.6*light,body && !wheel});
        }
        for(size_t i=0;i+2<mesh.indices.size();i+=3)faces.push_back({first+mesh.indices[i],first+mesh.indices[i+1],first+mesh.indices[i+2]});
    }
    if(points.empty())return;
    double minx=points[0].x,maxx=minx,miny=points[0].y,maxy=miny;
    for(auto &p:points){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);}
    double scale=std::min(w*.82/std::max(.01,maxx-minx),h*.78/std::max(.01,maxy-miny))*zoom;
    if(projection)*projection={scale*displayScale,width*.5+panX-(minx+maxx)*.5*scale*displayScale,height*.5+panY+(miny+maxy)*.5*scale*displayScale,yaw,pitch};
    for(auto &p:points){p.x=(p.x-(minx+maxx)*.5)*scale+w*.5+panX/displayScale;p.y=h*.5+panY/displayScale-(p.y-(miny+maxy)*.5)*scale;}
    std::vector<uint32_t> pixels(size_t(w)*h,0);std::vector<double> depth(size_t(w)*h,-1e30);
    auto edge=[](const Point &a,const Point &b,double x,double y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
    for(auto f:faces) {
        auto &a=points[f[0]],&b=points[f[1]],&c=points[f[2]];double area=edge(a,b,c.x,c.y);if(std::abs(area)<1e-8)continue;
        int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),x1=std::min(w-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
        int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),y1=std::min(h-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
            double u=edge(b,c,x+.5,y+.5)/area,v=edge(c,a,x+.5,y+.5)/area,t=1-u-v;if(u<0 || v<0 || t<0)continue;
            double z=u*a.z+v*b.z+t*c.z;size_t pos=size_t(y)*w+x;if(z<=depth[pos])continue;depth[pos]=z;
            std::array<double,4> tex{};
            if(decal && a.decal && decal->width && decal->height) {
                // Bilinear filtering in premultiplied alpha avoids colored fringes
                // around transparent PNG edges. Native UV0 is top-left oriented.
                double tu=u*a.u+v*b.u+t*c.u,tv=u*a.v+v*b.v+t*c.v;
                double tx=(tu-std::floor(tu))*decal->width-.5,ty=(tv-std::floor(tv))*decal->height-.5;
                int ix=int(std::floor(tx)),iy=int(std::floor(ty));double fx=tx-ix,fy=ty-iy;
                for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++) {
                    int sx=((ix+dx)%int(decal->width)+int(decal->width))%int(decal->width);
                    int sy=((iy+dy)%int(decal->height)+int(decal->height))%int(decal->height);
                    size_t offset=(size_t(sy)*decal->width+sx)*4;
                    double weight=(dx?fx:1-fx)*(dy?fy:1-fy),alpha=decal->pixels.at(offset+3)/255.;
                    for(int k=0;k<3;k++)tex[k]+=decal->pixels.at(offset+k)/255.*alpha*weight;
                    tex[3]+=alpha*weight;
                }
            }
            double lighting=u*a.light+v*b.light+t*c.light;
            uint32_t color=0xff000000;
            for(int k=0;k<3;k++) {
                double channel=(u*a.color[k]+v*b.color[k]+t*c.color[k])*(1-tex[3])+tex[k]*lighting;
                color|=uint32_t(std::clamp(channel*255.,0.,255.))<<(16-8*k);
            }
            pixels[pos]=color;
        }
    }
    if(hudPreview&&hudNodes){for(const auto&node:hudNodes->nodes){const Image*image=node.name=="node_window"?&hudPreview->multiplier:node.name=="node_backpanel"?&hudPreview->meter:nullptr;if(!image)continue;std::array<std::array<double,3>,4>quad;V corners[]={{-.5f,.5f,0},{.5f,.5f,0},{.5f,-.5f,0},{-.5f,-.5f,0}};for(int i=0;i<4;i++){auto v=rotate(transform(node.matrix,corners[i],true));quad[i]={(v.x-(minx+maxx)*.5)*scale+w*.5+panX/displayScale,h*.5+panY/displayScale-(v.y-(miny+maxy)*.5)*scale,v.z};}rasterHud(*image,quad,w,h,depth,pixels);}}
    if(hit){hit->width=w;hit->height=h;hit->scale=displayScale;hit->offsetX=(width-w*displayScale)*.5;hit->offsetY=(height-h*displayScale)*.5;hit->depth=std::move(depth);}
    auto *surface=cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(pixels.data()),CAIRO_FORMAT_ARGB32,w,h,w*4);
    cairo_save(cr);displayTransform();cairo_set_source_surface(cr,surface,0,0);cairo_paint(cr);cairo_restore(cr);cairo_surface_destroy(surface);
    if(attachments){
        cairo_save(cr);displayTransform();
        auto project=[&](V p){auto v=rotate(p);return V{F((v.x-(minx+maxx)*.5)*scale+w*.5+panX/displayScale),F(h*.5+panY/displayScale-(v.y-(miny+maxy)*.5)*scale),0};};
        for(size_t i=0;i<attachments->nodes.size();i++){
            const auto &node=attachments->nodes[i];bool hud=node.name=="node_window"||node.name=="node_backpanel";
            if(!hud&&node.name.rfind("nos",0)!=0)continue;
            bool active=int(i)==selectedNode;cairo_set_source_rgb(cr,active?1:.1,active?.55:.9,active?.1:1);cairo_set_line_width(cr,active?3:1.5);
            auto point=[&](V p){return project(transform(node.matrix,p,true));};
            auto origin=point({0,0,0});
            if(hud){V corners[]={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};for(int j=0;j<4;j++){auto v=point(corners[j]);if(j==0)cairo_move_to(cr,v.x,v.y);else cairo_line_to(cr,v.x,v.y);}cairo_close_path(cr);cairo_stroke(cr);}
            else {auto end=point({0,0,.6f});cairo_move_to(cr,origin.x,origin.y);cairo_line_to(cr,end.x,end.y);cairo_stroke(cr);cairo_arc(cr,end.x,end.y,3,0,6.283185);cairo_fill(cr);}
            cairo_arc(cr,origin.x,origin.y,4,0,6.283185);cairo_stroke(cr);cairo_move_to(cr,origin.x+7,origin.y-7);cairo_set_font_size(cr,13);
            auto label=node.name=="node_window"?"Multiplier":node.name=="node_backpanel"?"Boost meter":node.name.c_str();cairo_show_text(cr,label);
        }
        cairo_restore(cr);
    }

}
}
