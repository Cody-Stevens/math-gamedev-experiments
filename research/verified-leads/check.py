import re,os,glob,sys,json
GH=r'~\Documents\GitHub'; MATH=os.path.join(GH,'math')
REPOS=set(os.listdir(GH))
contents=open(os.path.join(MATH,'CONTENTS.md'),encoding='utf-8').read()
def norm(s):
    s=re.sub(r'\[a-zA-Z]+\*?','',s); s=re.sub(r'[^A-Za-z0-9]','',s); return s.lower()
NC=norm(contents)
texcache={}
def texnorm(d):
    if d not in texcache:
        buf=[]
        for f in glob.glob(os.path.join(MATH,'preprints',d,'**','*.tex'),recursive=True)+glob.glob(os.path.join(MATH,'preprints',d,'**','*.md'),recursive=True):
            try: buf.append(open(f,encoding='utf-8',errors='ignore').read())
            except: pass
        texcache[d]=norm('\n'.join(buf))
    return texcache[d]
rows=[]
for fp in sorted(glob.glob(os.path.join(os.path.dirname(__file__),'..','games','slice-*.md'))):
    t=open(fp,encoding='utf-8').read()
    for sec in re.split(r'\n(?=## G)',t)[1:]:
        fid=sec.split(':',1)[0][3:].strip(); title=sec.split('\n',1)[0]
        r={'id':fid,'title':title[3:].strip(),'issues':[]}
        pm=re.findall(r'preprints/([^/`\s]+)/([^`\s]*)',sec)
        dirs=list(dict.fromkeys(d for d,_ in pm))
        for d,f in pm:
            p=os.path.join(MATH,'preprints',d,f) if f else os.path.join(MATH,'preprints',d)
            if not os.path.exists(p): r['issues'].append(f'missing paper path {d}/{f}')
        if not dirs: r['issues'].append('no paper path')
        q=re.search(r'- Quote:\s*["“](.+?)["”]\s*$',sec,re.M)
        if q:
            nq=norm(q.group(1))
            ok=nq in NC or any(nq in texnorm(d) for d in dirs)
            if not ok:
                # partial: 80% prefix
                ok2=any(nq[:max(20,len(nq)*6//10)] in texnorm(d) for d in dirs) or nq[:max(20,len(nq)*6//10)] in NC
                r['issues'].append('quote partial-match only' if ok2 else 'QUOTE NOT FOUND')
        else: r['issues'].append('no quote')
        r['repos']=[]
        rows.append(r)
for r in rows: print(r['id'],'|',r['title'][:60],'|',','.join(r['repos']),'|','; '.join(r['issues']) or 'OK')
json.dump(rows,open(os.path.join(os.path.dirname(__file__),'mech.json'),'w'),indent=1)
