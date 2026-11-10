"""Reshape only the outer shore strata; retain inland platform vertices."""
from pathlib import Path
import subprocess,tempfile,math
R=Path(__file__).resolve().parents[1]
rel='场景工程_美术重制/assets/luoshen-environment/imported-props/riverbank-rocks/07_left_layered_shore.obj'
with tempfile.TemporaryFile() as f:
 subprocess.run(['git','show','80c3aae:'+rel],cwd=R,stdout=f,check=True);f.seek(0);lines=f.read().decode().splitlines()
verts=[list(map(float,l.split()[1:])) for l in lines if l.startswith('v ')]
assert len(verts)==422
original=[v[:] for v in verts]
for row in range(30):
 anchor=original[row*14+3][0]
 # Eroded coves interrupt the long projecting ledges, with alternating benches.
 retreat=.09+.19*(.5+.5*math.sin(row*1.83))
 for col in range(4,13):
  idx=row*14+col;x,y,z=verts[idx]
  x=anchor+(x-anchor)*.60-retreat
  if col in (5,6):y-=.035+.025*math.sin(row*1.4)
  if col in (7,8,9):x-=.055*(.5+.5*math.cos(row*2.1))
  verts[idx]=[x,y,z]
vi=0
for i,l in enumerate(lines):
 if l.startswith('v '):lines[i]='v '+' '.join(f'{x:.6f}' for x in verts[vi]);vi+=1
# Small irregular shoreline stones; bottom stays below water, no change to inland footprint.
for j,row in enumerate([1,2,3,8,11,15,20,24,27]):
 cx,_,cz=verts[row*14+10];cx+=.10
 rx=.16+.035*(j%3);rz=.19+.04*(j%2);top=.075+.025*(j%3)
 start=vi+1;lines+=['o Shore_Toe_'+str(j),'s off']
 pts=[]
 for level,y in [(0,-.20),(1,top)]:
  for k in range(6):
   angle=k*math.tau/6;radius=(.76 if level else 1)*(1+.12*math.sin(k*3+j))
   pts.append((cx+rx*radius*math.cos(angle),y+(0.012*math.sin(k+j) if level else 0),cz+rz*radius*math.sin(angle)))
 for p in pts:lines.append('v '+' '.join(f'{x:.6f}' for x in p))
 lines.append('usemtl Strata_Top')
 for k in range(1,5):lines.append(f'f {start+6} {start+6+k+1} {start+6+k}')
 lines.append('usemtl Strata_Blue')
 for k in range(6):
  n=(k+1)%6;lines.extend([f'f {start+k} {start+n} {start+6+n}',f'f {start+k} {start+6+n} {start+6+k}'])
 vi+=12
(R/rel).write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
assert all(verts[r*14+c]==original[r*14+c] for r in range(30) for c in [0,1,2,3,13])
print('Preserved all inland platform vertices; shortened outer ledges; added 9 toe rocks')
