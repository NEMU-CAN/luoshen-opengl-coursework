from pathlib import Path
import subprocess,tempfile,hashlib
R=Path(__file__).resolve().parents[1];rel='场景工程_美术重制/assets/luoshen-characters/luoshen/luoshen.obj'
with tempfile.TemporaryFile() as f:
 subprocess.run(['git','show','6ac622c:'+rel],cwd=R,stdout=f,check=True);f.seek(0);old=f.read().decode()
new=(R/rel).read_text(encoding='utf-8')
def objects(text):
 v=[];vt=[];vn=[];obj='';mat='';out={}
 for l in text.splitlines():
  if l.startswith('v '):v.append(l)
  elif l.startswith('vt '):vt.append(l)
  elif l.startswith('vn '):vn.append(l)
  elif l.startswith('o '):obj=l[2:];out.setdefault(obj,[])
  elif l.startswith('usemtl '):mat=l[7:]
  elif l.startswith('f '):
   corners=[]
   for token in l.split()[1:]:
    p=token.split('/');corners.append((v[int(p[0])-1],vt[int(p[1])-1] if len(p)>1 and p[1] else '',vn[int(p[2])-1] if len(p)>2 and p[2] else ''))
   out[obj].append((mat,corners))
 return out
a,b=objects(old),objects(new)
protected=[k for k in a if any(w in k.lower() for w in ('skirt','train','waist','ribbon','hands','drape','airy','sleeve_cloud','sleeve_far','sleeve_voile')) and a[k]]
assert protected
for k in protected:assert a[k]==b[k],f'Protected geometry changed: {k}'
# Accepted vertex-animation section must remain identical except unrelated face veil.
shader='场景工程_美术重制/graphics_support.h'
with tempfile.TemporaryFile() as f:
 subprocess.run(['git','show','6ac622c:'+shader],cwd=R,stdout=f,check=True);f.seek(0);s=f.read().decode()
t=(R/shader).read_text(encoding='utf-8')
start='    if(clothMotionEnabled==1 && materialKind>=26';end='    vec4 position = gl_ModelViewMatrix * local;'
assert s[s.index(start):s.index(end)]==t[t.index(start):t.index(end)],'Accepted cloth motion changed'
print('PASS: unchanged geometry/UV/normals/materials for',len(protected),'protected objects:')
print('\n'.join(protected));print('PASS: accepted clothing displacement shader unchanged')
