import sys,copy
from pathlib import Path
r=Path(__file__).resolve().parents[1];from compile_runtime import encode
out=r/'build/fixtures';out.mkdir(exist_ok=True)
def rect(a,b,c,d):return [[a,b],[c,b],[c,d],[a,d],[a,b]]
def poly(i,rings,kind=1,cap=4):return dict(id=i,kind=kind,rings=rings,source=f'fixture-{i}',name='Synthetic building',use='unknown',policy_basis='technical-fixture',confidence=.6,capacity_known=kind==1,capacity=cap if kind==1 else 0,opening_known=True,opens=0,closes=0)
nodes=[(101,[10,10]),(102,[12,10]),(103,[14,10]),(104,[12,12]),(105,[20,20]),(106,[22,20]),(107,[29,31]),(108,[27,31]),(109,[15,15])]
def edge(i,a,b,pts=None,q=True,**kw):return dict(id=i,**{'from':a,'to':b},points=pts or [dict(nodes)[a],dict(nodes)[b]],qualified=q,capacity=4,source=f'fixture-edge-{i}',basis='technical-fixture explicit walk qualification',**kw)
m=dict(width=4096,height=4096,generation='f'*64,basis='synthetic geometry fixtures; not Canterbury history',polygons=[poly(1,[rect(30,30,40,40),rect(33,33,37,37)]),poly(2,[rect(50,30,60,40)],cap=8),poly(3,[rect(70,30,80,40)],cap=0),poly(4,[rect(90,30,100,40)],2),poly(5,[rect(110,30,120,40)],3)],nodes=[dict(id=i,source=f'fixture-node-{i}',point=p) for i,p in nodes],edges=[edge(201,101,102),edge(202,102,103),edge(203,101,104,[[10,10],[10,12],[12,12]]),edge(204,104,103),edge(205,105,106),edge(206,108,107),edge(207,103,109,q=False)],doors=[dict(id=301,building=1,node=107,reviewed=True,basis='technical-fixture single exterior wall',points=[[29,31],[31,31]]),dict(id=302,building=1,node=107,reviewed=False,basis='technical candidate',points=[[29,31],[31,31]])])
# third dwelling has unknown capacity
m['polygons'][2]['capacity_known']=False
(out/'city.bin').write_bytes(encode(m))
for name,mutate in [
 ('duplicate',lambda c:c['nodes'][0].update(id=1)),
 ('bad-door',lambda c:c['doors'][0].update(points=[[29,31],[35,35]])),
 ('cut-wall',lambda c:c['edges'][0].update(points=[[10,10],[35,31],[12,10]])),
 ('bad-grade',lambda c:c['edges'][1].update(grade=1)),
 ('unknown-node',lambda c:c['edges'][0].update(to=999)),
]:
 c=copy.deepcopy(m);mutate(c);(out/(name+'.bin')).write_bytes(encode(c))
b=bytearray(encode(m));b[-1]^=1;(out/'bad-crc.bin').write_bytes(b)
# Reviewed technical connectors let human tests walk between two actual polygon footprints.
h=copy.deepcopy(m)
extra=[(110,[49,31]),(111,[47,31]),(112,[27,28]),(113,[47,28])]
nodes+=extra
h['nodes'] += [dict(id=i,source=f'fixture-node-{i}',point=p) for i,p in extra]
h['edges'] += [edge(208,108,112),edge(209,112,113),edge(210,113,111),edge(211,111,110)]
h['doors'].append(dict(id=303,building=2,node=110,reviewed=True,basis='technical-fixture exterior entrance',points=[[49,31],[51,31]]))
(out/'human.bin').write_bytes(encode(h))
w=copy.deepcopy(h)
w['sites']=[dict(id=401,source='closed-fixture',name='Closed Christian fixture',religion='Christian',denomination='source denomination',status='closed',point=[51,31],review_required=True,setting_year=1940,source_record='{"closure_year":1928,"fixture":true}'),dict(id=402,source='unlocated-fixture',name='Unlocated Jewish fixture',religion='Jewish',denomination='unknown',status='unknown',point=None,review_required=True,setting_year=1940,source_record='{"fixture":true,"date":"unknown"}'),dict(id=403,source='active-fixture',name='Active fixture',religion='Christian',denomination='unknown',status='active',point=[31,31],setting_year=1940,source_record='{"fixture":true}')]
(out/'work.bin').write_bytes(encode(w))
c=copy.deepcopy(w)
c['sites'][0]['denomination']='anglican'
c['sites'].append(dict(id=404,source='synagogue-fixture',name='Jewish scenario fixture',religion='judaism',denomination='unknown',status='scenario_active',point=[51,31],review_required=True,setting_year=1940,source_record='{"fixture":true,"setting_assignment":"explicit"}'))
(out/'civic.bin').write_bytes(encode(c))
