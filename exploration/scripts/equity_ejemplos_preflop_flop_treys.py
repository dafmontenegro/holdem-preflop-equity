import random, itertools
from treys import Card, Evaluator, Deck
ev=Evaluator()
def C(s): return [Card.new(x) for x in s.split()]
def equity(hero, board, n_opp, iters=40000):
    known=set(hero+board)
    rest=[c for c in Deck.GetFullDeck() if c not in known]
    w=t=0.0
    for _ in range(iters):
        random.shuffle(rest)
        opp=[rest[2*i:2*i+2] for i in range(n_opp)]
        need=5-len(board)
        bd=board+rest[2*n_opp:2*n_opp+need]
        h=ev.evaluate(bd,hero)
        best=min(ev.evaluate(bd,o) for o in opp)
        if h<best: w+=1
        elif h==best:
            k=1+sum(1 for o in opp if ev.evaluate(bd,o)==h); t+=1/k
    return (w+t)/iters
random.seed(1)
print("AA preflop vs N random")
for n in [1,2,3,5,8]: print(n, round(equity(C("As Ah"),[],n),3))
print("72o vs 1,5:", round(equity(C("7s 2h"),[],1),3), round(equity(C("7s 2h"),[],5),3))
print("AKs vs 1,5:", round(equity(C("As Ks"),[],1),3), round(equity(C("As Ks"),[],5),3))
b=C("Qs 7s 2d")
print("AsKs flop Qs7s2d vs 1,2,5:", [round(equity(C("As Ks"),b,n),3) for n in [1,2,5]])
print("KK on Ac 7d 2h vs 1:", round(equity(C("Ks Kh"),C("Ac 7d 2h"),1),3))
print("Top pair AhQd... 9c 6d 2s? hero Ah Qd vs 1,3:", round(equity(C("Ah Qd"),C("Qs 7c 2h"),1),3), round(equity(C("Ah Qd"),C("Qs 7c 2h"),3),3))
