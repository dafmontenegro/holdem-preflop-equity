import random, itertools, time
from treys import Card, Evaluator, Deck
ev=Evaluator(); random.seed(7)
hero=[Card.new("9c"),Card.new("8c")]
deck=[c for c in Deck.GetFullDeck() if c not in hero]
hands=list(itertools.combinations(deck,2))   # 1225
print("combos rivales:",len(hands))
N=2500
eq={}
t=time.time()
for h in hands:
    rest=[c for c in deck if c not in h]
    w=0.0
    for _ in range(N):
        b=random.sample(rest,5)
        a=ev.evaluate(b,hero); o=ev.evaluate(b,list(h))
        w+= 1 if a<o else (0.5 if a==o else 0)
    eq[h]=w/N
print("tiempo",round(time.time()-t))
vals=list(eq.values())
print("equity media vs mano concreta:",round(sum(vals)/len(vals),4))
fav=[h for h in hands if eq[h]<0.47]      # rival favorito
dud=[h for h in hands if eq[h]>0.53]
mid=[h for h in hands if 0.47<=eq[h]<=0.53]
print("rival favorito(<47%):",len(fav),round(len(fav)/1225,3),
      "| parejo:",len(mid),round(len(mid)/1225,3),"| tu favorito(>53%):",len(dud),round(len(dud)/1225,3))
S=set(fav)
# exacto para 2 rivales (pares disjuntos)
tot=both=one=0
for i,h1 in enumerate(hands):
    s1=set(h1)
    for h2 in hands[i+1:]:
        if s1 & set(h2): continue
        tot+=1
        k=(h1 in S)+(h2 in S)
        both+= k==2; one+= k==1
print("2 rivales exacto: ninguno",round((tot-both-one)/tot,3),"uno",round(one/tot,3),"ambos",round(both/tot,3))
# muestreo para 3 y 5 rivales
for n in (3,5,8):
    cnt=[0]*(n+1); M=200000
    for _ in range(M):
        d=random.sample(deck,2*n)
        k=sum(1 for i in range(n) if tuple(sorted(d[2*i:2*i+2])) in S or tuple(d[2*i:2*i+2]) in S)
        cnt[k]+=1
    print(n,"rivales: ninguno",round(cnt[0]/M,3),"al menos uno",round(1-cnt[0]/M,3),"| aprox independiente",round(1-(1-len(fav)/1225)**n,3))
# ejemplos de composicion
from collections import Counter
def cls(h): return Card.int_to_str(h[0])[0]+Card.int_to_str(h[1])[0]
pairs=[h for h in hands if Card.get_rank_int(h[0])==Card.get_rank_int(h[1])]
print("parejas de bolsillo rivales:",len(pairs),"| de ellas favoritas:",sum(1 for h in pairs if h in S))
print("equity vs 77:",round(eq[next(h for h in pairs if Card.int_to_str(h[0])[0]=='7')],3),
      "vs AA:",round(eq[next(h for h in pairs if Card.int_to_str(h[0])[0]=='A')],3))
