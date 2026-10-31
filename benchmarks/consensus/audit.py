"""Independent file-level audits; no placement optimization runs in Python."""
from pathlib import Path
from collections import defaultdict, Counter
import csv,json,math,re,shlex,sys

def read_state(path):
    d=dict(cells={},wells=defaultdict(dict),rows={},regions=defaultdict(list),nets={},pins=defaultdict(list))
    for line in Path(path).open():
        a=shlex.split(line);tag=a.pop(0)
        if tag=='GRID':d['pitch']=list(map(float,a))
        elif tag=='BOUNDS':d['bounds']=list(map(float,a))
        elif tag=='CELL':
            name,master,kind=a[:3];v=list(map(float,a[3:]));mov,o,x,y,w,nom,h,count,ns=v[:9]
            assert name not in d['cells'],('duplicate owner',name)
            assert int(ns)==len(v[9:])
            d['cells'][name]=dict(master=master,kind=kind,movable=bool(mov),orient=int(o),x=x,y=y,w=w,nom=nom,h=h,count=int(count),stretch=list(map(int,v[9:])))
        elif tag=='WELL':d['wells'][a[0]][int(a[1])]=list(map(float,a[2:]))
        elif tag=='ROW':d['rows'][tuple(map(int,a[:3]))]=list(map(float,a[3:]))
        elif tag=='REGION':d['regions'][a[3]].append((tuple(map(int,a[:3])),int(a[4])))
        elif tag=='NET':d['nets'][a[0]]=float(a[1])
        elif tag=='PIN':d['pins'][a[0]].append((a[1],*map(float,a[2:])))
    return d

def geometry(d,metadata):
    errors=[];eps=1e-7
    def check(ok,message):
        if not ok:errors.append(message)
    cells=d['cells'];logic={k:v for k,v in cells.items() if v['kind']=='component' and v['movable']}
    check(len(logic)==metadata['owners'],'owner inventory')
    check(len(d['nets'])==metadata['nets'],'net inventory')
    check(dict(sorted(Counter(str(v['count']) for v in logic.values()).items()))==metadata['height_counts'],'height inventory')
    bx,by,ux,uy=d['bounds'];gx,gy=d['pitch'];members=defaultdict(list)
    for name,c in cells.items():
        check(c['orient'] in (0,5),'unsupported orientation '+name)
        check(all(math.isfinite(c[k]) for k in ['x','y','w','h']),'nonfinite '+name)
        check(c['x']>=bx-eps and c['x']+c['w']<=ux+eps and c['y']>=by-eps and c['y']+c['h']<=uy+eps,'core boundary '+name)
    for name,c in logic.items():
        n=c['count'];s=c['stretch'];fs=c['orient']==5;occ=sorted(d['regions'][name],key=lambda x:x[1])
        check(len(s)==n-1 and all(v>=0 for v in s) and abs(c['h']-c['nom']-sum(s))<eps,'stretch shape '+name)
        check(len(occ)==n and [r for _,r in occ]==list(range(n)),'region inventory '+name)
        if len(occ)!=n:continue
        start=occ[0][0];actual_edges=[];nominal_edges=[]
        for key,r in occ:
            check(key==(start[0],start[1],start[2]+r),'region continuity '+name)
            if key not in d['rows']:check(False,'missing row '+name);continue
            row=d['rows'][key];lx,ly,w,h,p,nheight,nabove,lm,rm=row;members[key].append(c)
            check(c['x']>=lx+lm-eps and c['x']+c['w']<=lx+w-rm+eps,'row X '+name)
            k=n-1-r if fs else r
            if k not in d['wells'][name]:check(False,'missing well '+name);continue
            well=d['wells'][name][k];pw,nw=well[:4],well[4:];above=pw[3]==nw[1]
            edge=pw[3] if above else nw[3]
            check(above or nw[3]==pw[1],'NP pair '+name)
            check((not above if fs else above)==bool(nabove),'well orientation '+name)
            physical=c['y']+(c['nom']+sum(s)-edge-sum(s[:k]) if fs else edge+sum(s[:k]))
            rowedge=ly+(p if nabove else nheight)
            check(abs(physical-rowedge)<eps,'NP alignment '+name)
            check(pw[3]-pw[1]<=p+eps and nw[3]-nw[1]<=nheight+eps,'well containment '+name)
            actual_edges.append(rowedge);nominal_edges.append(c['nom']-edge if fs else edge)
            if r and len(actual_edges)==r+1:
                si=n-1-r if fs else r-1
                check(abs(s[si]-(actual_edges[r]-actual_edges[r-1]-(nominal_edges[r]-nominal_edges[r-1])))<eps,'stretch adjacency '+name)
        first=d['rows'][occ[0][0]];last=d['rows'][occ[-1][0]]
        check(c['y']>=first[1]-eps and c['y']+c['h']<=last[1]+last[3]+eps,'owner row union '+name)
    for key,row in d['rows'].items():
        lx,ly,w,h,p,n,orient,lm,rm=row
        check(lx>=bx and lx+w<=ux and ly>=by and ly+h<=uy,'row bounds '+str(key))
        mm=sorted(members[key],key=lambda c:c['x'])
        check(all(a['x']+a['w']<=b['x']+eps for a,b in zip(mm,mm[1:])),'row overlap '+str(key))
    # Independent Y buckets plus X sweep over full physical rectangles. Unlike
    # the historical nested all-pairs audit, this scales with local occupancy.
    bands=defaultdict(list);band=max(1.,max((r[3] for r in d['rows'].values()),default=12))
    boxes=[(name,c) for name,c in cells.items() if c['w']>0 and c['h']>0]
    for i,(_,c) in enumerate(boxes):
        for b in range(math.floor(c['y']/band),math.floor((c['y']+c['h']-eps)/band)+1):bands[b].append(i)
    overlaps=set()
    for ids in bands.values():
        ids.sort(key=lambda i:boxes[i][1]['x'])
        for ii,ai in enumerate(ids):
            an,a=boxes[ai]
            for jj in range(ii+1,len(ids)):
                bi=ids[jj]
                bn,b=boxes[bi]
                if b['x']>=a['x']+a['w']-eps:break
                if min(a['y']+a['h'],b['y']+b['h'])>max(a['y'],b['y'])+eps:overlaps.add(tuple(sorted((an,bn))))
    check(not overlaps,'full rectangle overlaps '+str(len(overlaps)))
    taps=[c for c in cells.values() if c['kind']=='tap'];check(len(taps)==2*len(d['rows']),'tap count')
    # The derived tap is exactly 12 sites high and occupies each row boundary.
    tap_index=defaultdict(list)
    for t in taps:tap_index[round(t['y'],7)].append(t)
    worst_gap=0.
    for key,row in d['rows'].items():
        lx,ly,w,h,p,n,orient,lm,rm=row
        tt=[t for t in tap_index[round(ly,7)] if lx-eps<=t['x'] and t['x']+t['w']<=lx+w+eps]
        check(len(tt)==2,'row tap inventory '+str(key))
        if len(tt)!=2:continue
        tt.sort(key=lambda c:c['x']);check(abs(tt[0]['x']-lx)<eps and abs(tt[1]['x']+tt[1]['w']-lx-w)<eps,'tap boundary '+str(key))
        check(all(t['orient']==(0 if orient else 5) and t['h']<=h+eps for t in tt),'tap geometry '+str(key))
        for c in members[key]:
            cx=(c['x']+.5*c['w'])*gx;cy=(ly+.5*h)*gy
            nearest=min(math.hypot(cx-(t['x']+.5*t['w'])*gx,cy-(t['y']+.5*t['h'])*gy) for t in tt)
            worst_gap=max(worst_gap,nearest)
            check(nearest<=metadata['max_plug_dist_um']+eps,'tap coverage')
    hp=[0.,0.];pin_error=0.
    for net,weight in d['nets'].items():
        xx=[];yy=[]
        for owner,ox,oy,ax,ay in d['pins'][net]:
            c=cells[owner];x=c['x']+ox;y=c['y']+oy
            xx.append(x);yy.append(y);pin_error=max(pin_error,abs(x-ax),abs(y-ay))
        if len(xx)>1:hp[0]+=weight*(max(xx)-min(xx))*gx;hp[1]+=weight*(max(yy)-min(yy))*gy
    check(pin_error<eps,'absolute pin geometry')
    return dict(violation_count=len(errors),errors=errors[:50],hpwl_x=hp[0],hpwl_y=hp[1],pin_coordinate_error=pin_error,max_tap_gap_um=worst_gap,row_copies=sum(c['count'] for c in logic.values()),stretched_owners=sum(any(v>0 for v in c['stretch']) for c in logic.values()))

def def_topology(path):
    text=Path(path).read_text();match=re.search(r'(?m)^[ \t]*NETS\s+(\d+)\s*;(.*?)END NETS',text,re.S)
    assert match,'missing NETS section'
    result=Counter();names=[]
    for m in re.finditer(r'(?m)^[ \t]*-\s+(\S+)\s*(.*?);',match[2],re.S):
        names.append(m[1])
        for owner,pin in re.findall(r'\(\s*(\S+)\s+(\S+)\s*\)',m[2]):result[(m[1],'io' if owner=='PIN' else 'component',pin if owner=='PIN' else owner,'PIN' if owner=='PIN' else pin)]+=1
    assert len(names)==int(match[1]) and len(set(names))==len(names)
    return set(names),result

def def_poses(path,section):
    text=Path(path).read_text()
    units=int(re.search(r'UNITS DISTANCE MICRONS (\d+)',text)[1])
    block=re.search(r'(?m)^'+section+r'\s+(\d+)\s*;(.*?)END '+section,text,re.S)
    assert block,'missing '+section
    result={}
    for record in re.finditer(r'(?m)^\s*-\s+(\S+)\s+(.*?);',block[2],re.S):
        name,body=record[1],record[2]
        pose=re.search(r'\+\s+(PLACED|FIXED|COVER)\s*\(\s*(-?\d+)\s+(-?\d+)\s*\)\s+(\S+)',body)
        assert pose and name not in result,('bad exported pose',name)
        result[name]=dict(master=body.split()[0] if section=='COMPONENTS' else '__PIN__',status=pose[1],x=int(pose[2])/units,y=int(pose[3])/units,orient=pose[4])
    assert len(result)==int(block[1])
    return result

def file_geometry(case_dir,run_dir,state,topology):
    # Verify DEF poses and original fixed anchors independently of the state
    # exporter. The paired state remains necessary to describe stretching.
    gx,gy=state['pitch'];cells=state['cells'];eps=1e-7
    exported=def_poses(run_dir/'placed.def','COMPONENTS')
    physical={k:v for k,v in cells.items() if v['w']>0 and v['h']>0}
    assert exported.keys()==physical.keys(),'DEF/state physical inventory'
    anchors=def_poses(case_dir/'input.def','COMPONENTS')
    ports=def_poses(case_dir/'input.def','PINS')
    final_ports=def_poses(run_dir/'placed.def','PINS')
    assert ports==final_ports,'changed exported IO anchors'
    for name,c in physical.items():
        p=exported[name]
        assert p['master']==c['master'] and p['orient']==('N' if c['orient']==0 else 'FS'),('export master/orientation',name)
        assert abs(p['x']-c['x']*gx)<eps and abs(p['y']-c['y']*gy)<eps,('export coordinate',name)
        if name in anchors and anchors[name]['status']=='FIXED':
            assert p==anchors[name],('changed fixed input macro',name)
    for name,p in ports.items():
        c=cells[name]
        assert not c['movable'] and c['w']==c['h']==0
        assert abs(p['x']-c['x']*gx)<eps and abs(p['y']-c['y']*gy)<eps,('IO state coordinate',name)
    assert def_topology(run_dir/'placed.def')==topology,'exported net topology'
    # Independently reconstruct point-pin offsets from the generated LEF.
    # Dali's present pin model reflects nominal LEF Y for FS, without pin
    # deformation under well stretching. This is an explicit abstraction.
    lef=(case_dir/'cells.lef').read_text();masters={}
    manufacturing=float(re.search(r'MANUFACTURINGGRID ([\d.eE+-]+)',lef)[1])
    def nearest(v):return math.floor(v+.5) if v>=0 else math.ceil(v-.5)
    def grid_length(v,pitch):return nearest(v/manufacturing)/nearest(pitch/manufacturing)
    for macro in re.finditer(r'(?m)^MACRO (\S+)\s*\n(.*?)(?=^MACRO |^END LIBRARY)',lef,re.S):
        size=re.search(r'\bSIZE ([\d.eE+-]+) BY ([\d.eE+-]+)',macro[2])
        pins={}
        for pin in re.finditer(r'(?m)^\s+PIN (\S+)\s*\n(.*?)(?=^\s+PIN |^END )',macro[2],re.S):
            rect=re.search(r'RECT ([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+)',pin[2])
            assert rect,('missing abstract pin rectangle',macro[1],pin[1])
            x1,y1,x2,y2=map(float,rect.groups());pins[pin[1]]=((grid_length(x1,gx)+grid_length(x2,gx))/2,(grid_length(y1,gy)+grid_length(y2,gy))/2)
        masters[macro[1]]=(float(size[2])/gy,pins)
    expected=Counter();actual=Counter()
    for (net,kind,name,pin),count in topology[1].items():
        c=cells[name]
        if kind=='io':ox=oy=0.
        else:
            height,pins=masters[c['master']];ox,oy=pins[pin]
            if c['orient']==5:oy=height-oy
        expected[(net,name,round(ox,6),round(oy,6))]+=count
    for net,pins in state['pins'].items():
        for name,ox,oy,ax,ay in pins:actual[(net,name,round(ox,6),round(oy,6))]+=1
    assert actual==expected,('LEF/topology/state pin geometry',list((actual-expected).items())[:5],list((expected-actual).items())[:5])
    return dict(exported_physical_cells=len(exported),fixed_ports=len(ports),verified_pin_connections=sum(actual.values()))

def audit(case_dir,run_dir,original_def):
    case_dir=Path(case_dir);run_dir=Path(run_dir);meta=json.loads((case_dir/'conversion.json').read_text())
    original=def_topology(original_def);derived=def_topology(case_dir/'input.def')
    assert original==derived,'conversion changed signal net-pin incidence'
    tsv=Counter(tuple(row[k] for k in ['net','kind','owner','pin']) for row in csv.DictReader((case_dir/'connectivity.tsv').open(),delimiter='\t'))
    assert tsv==derived[1] and sum(tsv.values())==meta['connections']
    before=read_state(run_dir/'placed.multi_region.source');after=read_state(run_dir/'placed.multi_region.final')
    a=geometry(before,meta);b=geometry(after,meta);pose_errors=[];changed=0
    assert before['cells'].keys()==after['cells'].keys()
    for name,c in before['cells'].items():
        trial=after['cells'][name]
        for key in c:
            if key!='x' and c[key]!=trial[key]:pose_errors.append((name,key))
        if c['kind']=='component' and c['movable']:changed+=c['x']!=trial['x']
        elif c['x']!=trial['x']:pose_errors.append((name,'fixed X'))
    if before['rows']!=after['rows'] or before['regions']!=after['regions'] or before['wells']!=after['wells']:pose_errors.append(('metadata','changed'))
    metrics=json.loads((run_dir/'metrics.json').read_text())['stages']
    error=max(abs(a['hpwl_x']-metrics['multi_region.initialization.x']),abs(a['hpwl_y']-metrics['multi_region.initialization.y']),abs(b['hpwl_x']-metrics['final.x']),abs(b['hpwl_y']-metrics['final.y']))
    tolerance=1e-7+2e-12*max(1.,metrics['final'])
    assert a['violation_count']==b['violation_count']==0,(a,b)
    assert not pose_errors,pose_errors[:20]
    assert error<=tolerance,(error,tolerance)
    assert metrics['final']<=metrics['multi_region.initialization']+tolerance
    files=file_geometry(case_dir,run_dir,after,derived)
    ledger=list(csv.DictReader((case_dir/'owners.tsv').open(),delimiter='\t'))
    assert len(ledger)==meta['owners']+meta['fixed_physical_macros']
    for row in ledger:
        c=after['cells'][row['owner']]
        assert c['master']==row['master'] and c['w']==float(row['width_sites']) and c['nom']==float(row['derived_height_sites'])
    result=dict(status='PASS',source=a,final=b,changed=changed,pose_errors=pose_errors,numerical_error_um=error,numerical_tolerance_um=tolerance,topology_connections=sum(tsv.values()),owners=meta['owners'],nets=meta['nets'],file_geometry=files)
    (run_dir/'independent_audit.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
if __name__=='__main__':
    print(json.dumps(audit(sys.argv[1],sys.argv[2],sys.argv[3]),indent=2))
