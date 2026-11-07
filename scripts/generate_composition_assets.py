"""Generate original lightweight papercraft framing props; no external downloads."""
from pathlib import Path
import math,random
R=Path(__file__).resolve().parents[1]/'场景工程_美术重制/assets/luoshen-environment/composition'
R.mkdir(exist_ok=True)
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def sub(a,b):return add(a,mul(b,-1))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a):return mul(a,1/math.sqrt(sum(x*x for x in a)))
class Mesh:
 def __init__(self):self.v=[];self.f=[]
 def face(self,pts,mat):
  start=len(self.v)+1;self.v.extend(pts);self.f.append((mat,list(range(start,start+len(pts)))))
 def write(self,name,colors):
  lines=['# Original course project prop','mtllib '+name+'.mtl']
  lines+=['v '+' '.join(f'{x:.6f}' for x in p) for p in self.v]
  for mat,face in self.f:
   lines+=['usemtl '+mat]
   for j in range(1,len(face)-1):lines+=['f %d %d %d'%(face[0],face[j],face[j+1])]
  (R/(name+'.obj')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
  (R/(name+'.mtl')).write_text('\n'.join('newmtl '+name+'\nKd '+' '.join(map(str,c))+'\nKs 0.03 0.03 0.03\nNs 12\n' for name,c in colors.items()),encoding='utf-8')
 def tube(self,a,b,r1,r2,mat,n=5):
  axis=unit(sub(b,a));side=unit(cross(axis,(0,0,1)));up=cross(axis,side)
  rings=[]
  for p,r in [(a,r1),(b,r2)]:rings.append([add(p,add(mul(side,r*math.cos(j*2*math.pi/n)),mul(up,r*math.sin(j*2*math.pi/n)))) for j in range(n)])
  for j in range(n):self.face([rings[0][j],rings[0][(j+1)%n],rings[1][(j+1)%n],rings[1][j]],mat)
  self.face(list(reversed(rings[0])),mat);self.face(rings[1],mat)
m=Mesh();rng=random.Random(71)
branches=[((0,0,0),(.23,1.55,.05),.31,.24),((.23,1.55,.05),(.90,2.7,.02),.24,.16),((.90,2.7,.02),(1.1,3.7,.0),.16,.075),((1.1,3.7,0),(1.7,4.25,.1),.075,.025),((.23,1.55,.05),(-.5,2.65,.10),.15,.075),((-.5,2.65,.10),(-1.1,3.30,.06),.075,.02),((.90,2.7,.02),(2.1,3.02,-.1),.13,.08),((2.1,3.02,-.1),(3.4,3.18,-.12),.08,.027),((3.4,3.18,-.12),(4.1,3.45,.03),.027,.008),((2.1,3.02,-.1),(2.5,3.75,.0),.06,.02),((2.5,3.75,0),(3.2,4.08,.08),.02,.007),((1.1,3.7,0),(.3,4.10,.2),.07,.025),((3.4,3.18,-.12),(3.3,3.7,.0),.025,.007),((.90,2.7,.02),(1.8,3.65,.35),.07,.012)]
colors={'bark':(.26,.20,.14),'petal':(.90,.81,.64),'petal_light':(.98,.91,.76),'petal_shadow':(.78,.68,.51),'pollen':(.68,.43,.19)}
for a,b,r1,r2 in branches:m.tube(a,b,r1,r2,'bark')
for a,b,r1,r2 in branches[3:]:
 for k in range(4):
  t=.4+.6*rng.random();p=add(a,mul(sub(b,a),t));p=add(p,(rng.uniform(-.14,.14),rng.uniform(-.11,.15),rng.uniform(-.10,.12)))
  radius=rng.uniform(.09,.15)
  for j in range(5):
   angle=j*math.tau/5;tip=add(p,(math.cos(angle)*radius,math.sin(angle)*radius,-.025))
   rim=[add(p,(math.cos(angle+offset)*radius*length,math.sin(angle+offset)*radius*length,-.025)) for offset,length in [(-.62,.55),(-.34,.93),(0,1.03),(.34,.93),(.62,.55)]]
   m.face([p]+rim,rng.choice(['petal','petal_light','petal_shadow']))
  m.face([add(p,(math.cos(j*math.tau/5)*.028,math.sin(j*math.tau/5)*.028,-.06)) for j in range(5)],'pollen')
m.write('blossom-frame',colors)
m=Mesh();poly=[(-1.7,-.65),(-.85,-1.05),(.70,-.97),(1.65,-.47),(1.8,.38),(.75,.91),(-.65,.84),(-1.6,.39)]
top=[(x,.85+.025*math.sin(j*2.3),z) for j,(x,z) in enumerate(poly)];bottom=[(x*.80,-.04,z*.83) for x,z in poly]
for j in range(8):
 m.face([(0,.85,0),top[(j+1)%8],top[j]],['stone_top','stone_top_light','stone_top'][j%3])
 mid=mul(add(add(top[j],top[(j+1)%8]),add(bottom[j],bottom[(j+1)%8])),.25)
 outward=unit((mid[0],0,mid[2]));mid=add(mid,mul(outward,.12))
 for face in [[top[j],top[(j+1)%8],mid],[top[(j+1)%8],bottom[(j+1)%8],mid],[bottom[(j+1)%8],bottom[j],mid],[bottom[j],top[j],mid]]:m.face(face,['stone','stone_shadow','stone_light'][j%3])
m.face(list(reversed(bottom)),'stone_shadow')
m.write('river-ledge',{'stone_top':(.48,.52,.46),'stone_top_light':(.53,.55,.48),'stone':(.38,.45,.43),'stone_shadow':(.29,.36,.36),'stone_light':(.44,.49,.45)})
m=Mesh();rings=[]
for z in [-1.35,1.35]:rings.append([(.92*math.cos(j*math.pi/8),1.05*math.sin(j*math.pi/8),z) for j in range(9)])
for j in range(8):m.face([rings[0][j],rings[1][j],rings[1][j+1],rings[0][j+1]],'bamboo_light' if j%3==0 else 'bamboo')
for j in range(8):m.face([(0,0,-1.35),rings[0][j+1],rings[0][j]],'bamboo')
for z in [-1.35,1.35]:
 for j in range(8):m.tube(rings[0 if z<0 else 1][j],rings[0 if z<0 else 1][j+1],.022,.022,'rim',4)
m.write('boat-canopy',{'bamboo':(.67,.51,.31),'bamboo_light':(.77,.62,.40),'rim':(.45,.32,.18)})
print('Generated blossom-frame, river-ledge and boat-canopy OBJ/MTL')
