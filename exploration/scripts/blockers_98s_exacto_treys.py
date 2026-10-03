import itertools, time
from collections import Counter
from multiprocessing import Pool
from treys import Card, Evaluator, Deck
ev=Evaluator()
C=lambda s:[Card.new(x) for x in s.split()]
FULL=Deck.GetFullDeck()

def job(a):
    hero,opp,i=a
    dead=set(hero)|set(opp)
    deck=[c for c in FULL if c not in dead]
    cnt=Counter(); w=t=l=0
    first=deck[i]
    for rest in itertools.combinations(deck[i+1:],4):
        b=[first]+list(rest)
        h=ev.evaluate(b,hero)
        cnt[ev.get_rank_class(h)]+=1
        if opp:
            o=ev.evaluate(b,opp)
            if h<o: w+=1
            elif h==o: t+=1
            else: l+=1
    return cnt,w,t,l

def run(hero,opp=()):
    dead=set(hero)|set(opp)
    n=len([c for c in FULL if c not in dead])
    with Pool() as p:
        res=p.map(job,[(hero,list(opp),i) for i in range(n-4)],chunksize=1)
    cnt=Counter(); W=T=L=0
    for c,w,t,l in res: cnt.update(c); W+=w;T+=t;L+=l
    tot=sum(cnt.values())
    P=lambda k:sum(cnt[c] for c in range(1,k+1))/tot
    d=dict(tot=tot,straight_plus=P(5),flush_plus=P(4),trips_plus=P(6),twopair_plus=P(7),full_plus=P(3))
    if opp: d.update(win=W/tot,tie=T/tot,lose=L/tot,eq=(W+T/2)/tot)
    return d
def show(name,d): print(name,{k:(round(v,4) if isinstance(v,float) else v) for k,v in d.items()},flush=True)

t0=time.time()
for nm,h in [("9c8c","9c 8c"),("7h2s","7h 2s"),("AsAh","As Ah"),("KsQs","Ks Qs")]:
    show(nm,run(C(h))); 
print("t",round(time.time()-t0),flush=True)
print("--- 9c8c contra manos reveladas ---")
for nm,o in [("vs AhAs (neutro)","Ah As"),("vs AcKc (2 treboles)","Ac Kc"),("vs 7h7d (bloquea escalera)","7h 7d"),("vs 7c6c (treboles+conectadas)","7c 6c")]:
    show(nm,run(C("9c 8c"),C(o)))
print("t",round(time.time()-t0),flush=True)
