#pragma once
#include "core.hpp"
namespace livery {
struct CarHudPreview {Image multiplier,meter;};
inline Image hudCell(const Image&atlas,int column,int row,int width,int height){require(column>=0&&row>=0&&(column+1)*width<=int(atlas.width)&&(row+1)*height<=int(atlas.height),"HUD atlas cell outside image");Image cell{uint32_t(width),uint32_t(height),Bytes(size_t(width)*height*4)};for(int y=0;y<height;y++)std::copy_n(atlas.pixels.data()+(size_t(row*height+y)*atlas.width+column*width)*4,width*4,cell.pixels.data()+size_t(y)*width*4);return cell;}
inline void hudOver(Image&base,const Image&layer){require(base.width==layer.width&&base.height==layer.height,"HUD layer dimensions differ");for(size_t i=0;i<base.pixels.size();i+=4){double a=layer.pixels[i+3]/255.,b=base.pixels[i+3]/255.,alpha=a+b*(1-a);for(int k=0;k<3;k++)base.pixels[i+k]=alpha>0?uint8_t(std::lround((layer.pixels[i+k]*a+base.pixels[i+k]*b*(1-a))/alpha)):0;base.pixels[i+3]=uint8_t(std::lround(alpha*255));}}
// Reuses the native atlas cells and unit attachment footprints from
// gaVehicleVisNode.tickWindowHud / tickPanelHud. Static x4, 70% charge;
// track tint, glow animation and gameplay visibility are not simulated.
inline CarHudPreview carHudPreview(const Bytes&batch){CarHudPreview result;for(auto&t:decodeTextures(batch)){if(t.name=="multiplier"){require(t.width==384&&t.height==288,"Unsupported multiplier atlas");auto atlas=decodeDDS(t);result.multiplier=hudCell(atlas,0,0,96,96);hudOver(result.multiplier,hudCell(atlas,1,1,96,96));}else if(t.name=="car_meter"){require(t.width==512&&t.height==768,"Unsupported boost meter atlas");auto atlas=decodeDDS(t);result.meter=hudCell(atlas,0,0,256,64);for(int row=1;row<=7;row++)hudOver(result.meter,hudCell(atlas,0,row,256,64));}}require(result.multiplier.width&&result.meter.width,"Car HUD atlas is incomplete");return result;}
// Orthographic textured quad, depth-tested against the same car surface used
// for picking. Alpha blends over the model without replacing its picking depth.
inline void rasterHud(const Image&image,const std::array<std::array<double,3>,4>&quad,int w,int h,const std::vector<double>&depth,std::vector<uint32_t>&pixels){
 double ax=quad[1][0]-quad[0][0],ay=quad[1][1]-quad[0][1],bx=quad[3][0]-quad[0][0],by=quad[3][1]-quad[0][1],det=ax*by-ay*bx;if(std::abs(det)<1e-8)return;
 double minx=quad[0][0],maxx=minx,miny=quad[0][1],maxy=miny;for(auto&p:quad){minx=std::min(minx,p[0]);maxx=std::max(maxx,p[0]);miny=std::min(miny,p[1]);maxy=std::max(maxy,p[1]);}
 int x0=int(std::clamp(std::floor(minx),0.,double(w))),x1=int(std::clamp(std::ceil(maxx),-1.,double(w-1))),y0=int(std::clamp(std::floor(miny),0.,double(h))),y1=int(std::clamp(std::ceil(maxy),-1.,double(h-1)));
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){double dx=x+.5-quad[0][0],dy=y+.5-quad[0][1],u=(dx*by-dy*bx)/det,v=(ax*dy-ay*dx)/det;if(u<0||u>1||v<0||v>1)continue;double z=quad[0][2]+u*(quad[1][2]-quad[0][2])+v*(quad[3][2]-quad[0][2]);size_t offset=size_t(y)*w+x;if(z<depth[offset]-1e-5)continue;
 double tx=u*image.width-.5,ty=v*image.height-.5;int ix=int(std::floor(tx)),iy=int(std::floor(ty));double fx=tx-ix,fy=ty-iy;std::array<double,4>sample{};for(int j=0;j<2;j++)for(int i=0;i<2;i++){size_t p=(size_t(std::clamp(iy+j,0,int(image.height)-1))*image.width+std::clamp(ix+i,0,int(image.width)-1))*4;double weight=(i?fx:1-fx)*(j?fy:1-fy),a=image.pixels[p+3]/255.;for(int k=0;k<3;k++)sample[k]+=image.pixels[p+k]*a*weight;sample[3]+=a*weight;}if(sample[3]<=0)continue;uint32_t old=pixels[offset],color=uint32_t(std::lround(255*sample[3]+(old>>24)*(1-sample[3])))<<24;for(int k=0;k<3;k++){int shift=16-k*8;double value=sample[k]+((old>>shift)&255)*(1-sample[3]);color|=uint32_t(std::clamp(std::lround(value),0l,255l))<<shift;}pixels[offset]=color;
 }
}
}
