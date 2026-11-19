#pragma once
#include "mod_editor.hpp"
namespace livery {
namespace snap {
inline V vAdd(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V vSub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V vMul(V a,double s){return {F(a.x*s),F(a.y*s),F(a.z*s)};}
inline double vDot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V vCross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline V vUnit(V a){double len=length(a);require(len>1e-9,"Degenerate snap direction");return vMul(a,1/len);}
struct Triangle{V a,b,c,n;};
inline V closest(V p,const Triangle &t){
 auto ab=vSub(t.b,t.a),ac=vSub(t.c,t.a),ap=vSub(p,t.a);double d1=vDot(ab,ap),d2=vDot(ac,ap);if(d1<=0&&d2<=0)return t.a;
 auto bp=vSub(p,t.b);double d3=vDot(ab,bp),d4=vDot(ac,bp);if(d3>=0&&d4<=d3)return t.b;
 double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return vAdd(t.a,vMul(ab,d1/(d1-d3)));
 auto cp=vSub(p,t.c);double d5=vDot(ab,cp),d6=vDot(ac,cp);if(d6>=0&&d5<=d6)return t.c;
 double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return vAdd(t.a,vMul(ac,d2/(d2-d6)));
 double va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return vAdd(t.b,vMul(vSub(t.c,t.b),(d4-d3)/((d4-d3)+(d5-d6))));
 double inv=1/(va+vb+vc);return vAdd(t.a,vAdd(vMul(ab,vb*inv),vMul(ac,vc*inv)));
}
inline double ray(V origin,V direction,const Triangle &t){auto e1=vSub(t.b,t.a),e2=vSub(t.c,t.a),h=vCross(direction,e2);double det=vDot(e1,h);if(std::abs(det)<1e-9)return -1;auto s=vSub(origin,t.a);double u=vDot(s,h)/det;if(u<0||u>1)return -1;auto q=vCross(s,e1);double v=vDot(direction,q)/det;if(v<0||u+v>1)return -1;double d=vDot(e2,q)/det;return d>1e-4?d:-1;}
inline std::array<double,9> align(const Model &model,const Matrix &base,const std::array<double,9> &values,bool hud){
 std::vector<Triangle> triangles;V center{};size_t count=0;
 for(const auto &node:model.nodes)if(bodyNode(node)&&lower(node.name).find("wheel")==std::string::npos)for(int index:node.meshes){const auto &mesh=model.meshes.at(index);auto materialName=lower(model.materials.at(mesh.material).at("name").get<std::string>());if(materialName.find("interior")!=std::string::npos||materialName.find("tyre")!=std::string::npos||materialName.find("wheel")!=std::string::npos)continue;
 for(size_t i=0;i+2<mesh.indices.size();i+=3){auto a=transform(node.matrix,mesh.positions.at(mesh.indices[i]),true),b=transform(node.matrix,mesh.positions.at(mesh.indices[i+1]),true),c=transform(node.matrix,mesh.positions.at(mesh.indices[i+2]),true);auto normal=vCross(vSub(b,a),vSub(c,a));if(length(normal)<1e-8)continue;triangles.push_back({a,b,c,vUnit(normal)});center=vAdd(center,a);++count;}}
 require(!triangles.empty(),"No body surface available for snapping");center=vMul(center,1./count);V position{F(values[0]),F(values[1]),F(values[2])};
 struct Candidate{double distance;size_t index;V point;};std::vector<Candidate> candidates;
 for(size_t i=0;i<triangles.size();i++){auto point=closest(position,triangles[i]);candidates.push_back({vDot(vSub(point,position),vSub(point,position)),i,point});}
 std::sort(candidates.begin(),candidates.end(),[](auto &a,auto &b){return a.distance<b.distance;});V point{},normal{};bool found=false;
 for(size_t i=0;i<std::min<size_t>(candidates.size(),256)&&!found;i++){auto c=candidates[i];auto n=triangles[c.index].n;if(vDot(n,vSub(c.point,center))<0)n=vMul(n,-1);
 for(int side=0;side<2&&!found;side++){auto direction=side?vMul(n,-1):n;bool blocked=false;auto origin=vAdd(c.point,vMul(direction,.0005));for(const auto &t:triangles)if(ray(origin,direction,t)>0){blocked=true;break;}if(!blocked){point=c.point;normal=direction;found=true;}}}
 require(found,"No exposed exterior surface nearby; move the element closer to the body");auto matrix=adjustedMatrix(base,values);V from=vUnit({matrix[6],matrix[7],matrix[8]});double cosine=std::clamp(vDot(from,normal),-1.,1.);auto axis=vCross(from,normal);double sine=length(axis);
 if(sine<1e-8){axis=vUnit(vCross(from,std::abs(from.x)<.8?V{1,0,0}:V{0,1,0}));}else axis=vMul(axis,1/sine);
 auto rotate=[&](V v){return vAdd(vAdd(vMul(v,cosine),vMul(vCross(axis,v),sine)),vMul(axis,vDot(axis,v)*(1-cosine)));};
 Matrix identity{1,0,0,0,1,0,0,0,1,0,0,0};auto angles=values;angles[6]=angles[7]=angles[8]=1;auto rotation=adjustedMatrix(identity,angles);for(int col=0;col<3;col++){auto v=rotate({rotation[col*3],rotation[col*3+1],rotation[col*3+2]});rotation[col*3]=v.x;rotation[col*3+1]=v.y;rotation[col*3+2]=v.z;}
 auto result=values;double yaw=std::asin(std::clamp(-double(rotation[2]),-1.,1.));double pitch=std::atan2(rotation[5],rotation[8]),roll=std::atan2(rotation[1],rotation[0]);if(std::abs(std::cos(yaw))<1e-6){pitch=0;roll=std::atan2(-rotation[3],rotation[4]);}result[3]=pitch*180/3.141592653589793;result[4]=yaw*180/3.141592653589793;result[5]=roll*180/3.141592653589793;
 double clearance=.02;
 // Lift the complete HUD footprint above local curvature, not only its center.
 if(hud){auto aligned=adjustedMatrix(base,result);V right{aligned[0],aligned[1],aligned[2]},up{aligned[3],aligned[4],aligned[5]};for(int y=0;y<5;y++)for(int x=0;x<5;x++){auto sample=vAdd(point,vAdd(vMul(right,(x-2)*.25),vMul(up,(y-2)*.25)));auto origin=vSub(sample,vMul(normal,.25));for(const auto &t:triangles){double d=ray(origin,normal,t);if(d>0&&d<.75)clearance=std::max(clearance,d-.25+.02);}}}
 point=vAdd(point,vMul(normal,clearance));result[0]=point.x;result[1]=point.y;result[2]=point.z;return result;
}
}
}
