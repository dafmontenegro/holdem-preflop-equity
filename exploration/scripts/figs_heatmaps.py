import csv, numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
d={}
for r in csv.reader(open('raw.csv')):
    key=r[0] if r[1]=="par" else r[0]+("s" if r[1]=="suited" else "o")
    n=int(r[2]); k=[int(x) for x in r[3:12]]
    d[key]=dict(esc=sum(k[4:])/n,col=sum(k[5:])/n,trio=sum(k[3:])/n)
R="AKQJT98765432"
def grid(f):
    g=np.zeros((13,13)); lab=[[None]*13 for _ in range(13)]
    for i,a in enumerate(R):
        for j,b in enumerate(R):
            key=a+a if i==j else (a+b+"s" if i<j else b+a+"o")
            g[i,j]=d[key][f]*100; lab[i][j]=key
    return g,lab
for f,t,fn in [("esc","Escalera o mejor al river (%)","h_esc.png"),("col","Color o mejor al river (%)","h_col.png"),("trio","Trío o mejor al river (%)","h_trio.png")]:
    g,lab=grid(f)
    fig,ax=plt.subplots(figsize=(7.2,6.6),dpi=160)
    im=ax.imshow(g,cmap="RdYlGn")
    for i in range(13):
        for j in range(13):
            ax.text(j,i,f"{lab[i][j]}\n{g[i,j]:.1f}",ha="center",va="center",fontsize=5.6)
    ax.set_xticks(range(13));ax.set_xticklabels(list(R));ax.set_yticks(range(13));ax.set_yticklabels(list(R))
    ax.set_title(t+"\narriba de la diagonal: suited · diagonal: parejas · abajo: offsuit",fontsize=9)
    fig.colorbar(im,fraction=0.046,pad=0.03); fig.tight_layout(); fig.savefig(fn); plt.close()
# figura estructura 169
fig,ax=plt.subplots(figsize=(6,5.6),dpi=160)
c=np.zeros((13,13))
for i in range(13):
    for j in range(13): c[i,j]=0 if i==j else (1 if i<j else 2)
ax.imshow(c,cmap=matplotlib.colors.ListedColormap(["#F4B183","#9DC3E6","#C9C9C9"]))
for i,a in enumerate(R):
    for j,b in enumerate(R):
        key=a+a if i==j else (a+b+"s" if i<j else b+a+"o")
        ax.text(j,i,key,ha="center",va="center",fontsize=6.5)
ax.set_xticks([]);ax.set_yticks([])
ax.set_title("Las 169 manos: 13 parejas (naranja), 78 suited (azul), 78 offsuit (gris)",fontsize=9)
fig.tight_layout(); fig.savefig("grid169.png"); plt.close()
print("ok")
