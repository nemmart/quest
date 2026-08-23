import re
DIS='/home/claude/proj/Disassembled/quest.dis'
CS='/home/claude/questrepo/Disassembled/quest.callsites'
BOOK='/home/claude/questrepo/Work/c_src/quest.addrbook'

# 1. book: name -> (alloc_base, wfp, max_argc)
book={}
for l in open(BOOK):
    if l.startswith('#'): continue
    p=l.split()
    if len(p)<6: continue
    pc,name,alloc,wfp,argc=p[0],p[1],int(p[2],16),int(p[3],16),int(p[4])
    book[name.split('@')[0]]=(alloc,wfp,argc,pc.upper())

# 2. dis lines indexed
lines=[l.rstrip() for l in open(DIS)]
idx={}
for k,l in enumerate(lines):
    m=re.match(r'([0-9a-f]{8}) ',l)
    if m: idx[m.group(1).upper()]=k
def op_at(pc):
    l=lines[idx[pc]]; return l.split()[1] if len(l.split())>1 else '?'

# 3. callsites: parse, classify
xcall=set(m.group(1).upper() for l in open(DIS) for m in [re.match(r'([0-9a-f]{8}) XCALL',l)] if m)
sites=[]
for l in open(CS):
    m=re.match(r'call (\S+),(\d+) at ([0-9A-F]+) (CLEAN(?:-EMPTY)?)',l)
    if m: sites.append((m.group(1),int(m.group(2)),m.group(3).upper(),m.group(4)))

# 4. For each site, find its arg-push pcs: walk back from the call over the window,
#    collecting push-class ops until we've found argc args (accounting WPSH width).
pushops={'XPEF':1,'LPEF':1,'XPEFB':1,'LPEFB':1}
def wpsh_width(pc):
    m=re.match(r'[0-9a-f]{8} WPSH (\d),(\d)',lines[idx[pc]])
    xx,aa=int(m.group(1)),int(m.group(2)); return (aa-xx)%4+1

def arg_pushes_for(callpc, argc):
    # walk back, collect pushes until sum of widths == argc
    k=idx[callpc]; got=0; found=[]  # list of (pc, width, opcode)
    j=k-1
    while j>=0 and got<argc and (k-j)<30:
        l=lines[j]; parts=l.split(); op=parts[1] if len(parts)>1 else ''
        pc=l[:8].upper()
        if op in pushops:
            found.append((pc,1,op)); got+=1
        elif op=='WPSH':
            w=wpsh_width(pc); found.append((pc,w,op)); got+=w
        # stop if we hit a call/branch boundary (shouldn't, windows are CLEAN)
        if (k-j)>1 and re.match(r'[0-9a-f]{8} ([LX]CALL|WBR|WRTN|WSAVS)',l): break
        j-=1
    found.reverse()
    return found, got

# 5. classify + generate for tranche A (flat LCALL, single-word only) and B (flat + WPSH)
A=[]; B=[]; C=0; D=0; EMPTY=0; SKIP=[]
for name,argc,callpc,clean in sites:
    if clean=='CLEAN-EMPTY': EMPTY+=1; continue
    if name=='RETURN_MESSAGE': D+=1; continue
    if callpc in xcall: C+=1; continue
    if name not in book: SKIP.append((name,callpc,'not-in-book')); continue
    alloc,wfp,maxargc,_=book[name]
    pushes,got=arg_pushes_for(callpc,argc)
    if got!=argc:
        SKIP.append((name,callpc,f'argc {argc} but found {got}')); continue
    has_wpsh=any(o=='WPSH' for _,_,o in pushes)
    # slot addresses: arg N at wfp-10-2N ... argmap says arg1 is nearest call (highest pc).
    # Assign slots: args in stack order. wfp-10 = marker. arg i (1..argc) at wfp-10-2*i? 
    # book comment: "arg N at wfp-10-2N", arg1 at wfp-12, arg2 at wfp-14... 
    # push order from low pc to call: last pushed (nearest call) is arg1.
    # We'll assign by the reversed found list: pushes[] is in pc order (low->high);
    # the HIGHEST-pc push is arg1 (nearest call). Slot(argi)=wfp-10-2*i.
    # Build per-word slots (WPSH expands to consecutive args).
    entries=[]
    # number args from the call backwards: nearest push = arg1
    # pushes is low->high pc; reverse to high->low = arg1,arg2,...
    argno=0; ok=True
    for pc,w,op in reversed(pushes):
        # a width-w push covers args argno+1..argno+w, but for WPSH the register order
        # means the FIRST word is the lowest arg number in that push. We assign the
        # push's base slot = slot of its highest arg number in this group.
        base_argno=argno+1; top_argno=argno+w
        # slot for the group's first-written word = wfp-10-2*top? Keep simple: the push
        # writes w consecutive slots; base area addr = wfp-10-2*top_argno (lowest addr),
        # ascending. Record base; loader validates in-range.
        base_slot=wfp-10-2*top_argno
        entries.append((pc,base_slot,w,op,base_argno,top_argno))
        argno+=w
    marker_slot=wfp-10
    rec={'name':name,'callpc':callpc,'argc':argc,'entries':entries,'marker':marker_slot,'wpsh':has_wpsh}
    (B if has_wpsh else A).append(rec)

print(f"Tranche A (flat, single-word): {len(A)} sites")
print(f"Tranche B (flat, WPSH multi):  {len(B)} sites")
print(f"C(XCALL)={C} D(RETMSG)={D} EMPTY={EMPTY} SKIP={len(SKIP)}")
if SKIP:
    print("SKIPPED:"); 
    for s in SKIP[:15]: print("  ",s)
# stash for writing
import json
json.dump({'A':A,'B':B},open('/tmp/pushmap_AB.json','w'))
