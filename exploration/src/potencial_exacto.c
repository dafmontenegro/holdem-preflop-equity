#include <stdio.h>
#include <string.h>
// carta = rango*4+palo ; rango 0..12 (2..A), palo 0..3
static int has_straight(int m){ // m: bitmask 13 bits
    int e=(m<<1)|((m>>12)&1);   // As tambien como rango bajo
    return (e&(e>>1)&(e>>2)&(e>>3)&(e>>4))!=0;
}
// categorias: 0 alta,1 pareja,2 doble,3 trio,4 escalera,5 color,6 full,7 poker,8 esc.color
static int cat(const int *c,int n){
    int cnt[13]={0},sc[4]={0},sm[4]={0},rm=0;
    for(int i=0;i<n;i++){int r=c[i]>>2,s=c[i]&3;cnt[r]++;sc[s]++;sm[s]|=1<<r;rm|=1<<r;}
    int quads=0,trips=0,pairs=0;
    for(int r=0;r<13;r++){ if(cnt[r]==4)quads++; else if(cnt[r]==3)trips++; else if(cnt[r]==2)pairs++; }
    int fs=-1; for(int s=0;s<4;s++) if(sc[s]>=5) fs=s;
    if(fs>=0 && has_straight(sm[fs])) return 8;
    if(quads) return 7;
    if(trips>=2 || (trips>=1 && pairs>=1)) return 6;
    if(fs>=0) return 5;
    if(has_straight(rm)) return 4;
    if(trips) return 3;
    if(pairs>=2) return 2;
    if(pairs==1) return 1;
    return 0;
}
int main(){
    const char *R="23456789TJQKA";
    for(int hi=12;hi>=0;hi--) for(int lo=hi;lo>=0;lo--){
        for(int suited=0;suited<2;suited++){
            if(hi==lo && suited) continue;
            int h[2]; h[0]=hi*4+0; h[1]=lo*4+(suited?0:1);
            int deck[50],nd=0;
            for(int c=0;c<52;c++) if(c!=h[0]&&c!=h[1]) deck[nd++]=c;
            long long k[9]={0}, mej=0, tot=0;
            int cards[7]; cards[0]=h[0]; cards[1]=h[1];
            for(int a=0;a<nd;a++)for(int b=a+1;b<nd;b++)for(int c=b+1;c<nd;c++)for(int d=c+1;d<nd;d++)for(int e=d+1;e<nd;e++){
                cards[2]=deck[a];cards[3]=deck[b];cards[4]=deck[c];cards[5]=deck[d];cards[6]=deck[e];
                int c7=cat(cards,7), c5=cat(cards+2,5);
                k[c7]++; if(c7>c5) mej++; tot++;
            }
            const char *tipo = hi==lo?"par":(suited?"suited":"offsuit");
            printf("%c%c,%s,%lld",R[hi],R[lo],tipo,tot);
            for(int i=0;i<9;i++) printf(",%lld",k[i]);
            printf(",%lld\n",mej);
        }
    }
}
