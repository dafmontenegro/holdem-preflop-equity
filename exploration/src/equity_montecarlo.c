#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static uint64_t s=88172645463325252ULL;
static inline uint64_t rnd(){s^=s<<13;s^=s>>7;s^=s<<17;return s;}
static int topstraight(int m){int e=(m<<1)|((m>>12)&1);int x=e&(e>>1)&(e>>2)&(e>>3)&(e>>4);if(!x)return -1;int b=31-__builtin_clz(x);return b+3;} // returns rank index of top card (0..12)
static int eval7(const int*c){
  int cnt[13]={0},sc[4]={0},sm[4]={0},rm=0;
  for(int i=0;i<7;i++){int r=c[i]>>2,su=c[i]&3;cnt[r]++;sc[su]++;sm[su]|=1<<r;rm|=1<<r;}
  #define S(cat,a,b,c_,d,e) ((cat)<<20|(a)<<16|(b)<<12|(c_)<<8|(d)<<4|(e))
  int fs=-1;for(int k=0;k<4;k++)if(sc[k]>=5)fs=k;
  if(fs>=0){int t=topstraight(sm[fs]);if(t>=0)return S(8,t,0,0,0,0);}
  int q=-1,t1=-1,t2=-1,p1=-1,p2=-1,p3=-1;
  for(int r=12;r>=0;r--){ if(cnt[r]==4)q=r; else if(cnt[r]==3){if(t1<0)t1=r;else if(t2<0)t2=r;} else if(cnt[r]==2){if(p1<0)p1=r;else if(p2<0)p2=r;else if(p3<0)p3=r;} }
  if(q>=0){int k=-1;for(int r=12;r>=0;r--)if(r!=q&&cnt[r]){k=r;break;}return S(7,q,k,0,0,0);}
  if(t1>=0&&(t2>=0||p1>=0)){int p=t2>p1?t2:p1;return S(6,t1,p,0,0,0);}
  if(fs>=0){int v[5],n=0;for(int r=12;r>=0&&n<5;r--)if(sm[fs]>>r&1)v[n++]=r;return S(5,v[0],v[1],v[2],v[3],v[4]);}
  int st=topstraight(rm);if(st>=0)return S(4,st,0,0,0,0);
  int k[5],n=0;
  if(t1>=0){for(int r=12;r>=0&&n<2;r--)if(r!=t1&&cnt[r])k[n++]=r;return S(3,t1,k[0],k[1],0,0);}
  if(p2>=0){int kk=-1;for(int r=12;r>=0;r--)if(r!=p1&&r!=p2&&cnt[r]){kk=r;break;}return S(2,p1,p2,kk,0,0);}
  if(p1>=0){for(int r=12;r>=0&&n<3;r--)if(r!=p1&&cnt[r])k[n++]=r;return S(1,p1,k[0],k[1],k[2],0);}
  for(int r=12;r>=0&&n<5;r--)if(cnt[r])k[n++]=r;return S(0,k[0],k[1],k[2],k[3],k[4]);
}
int main(int argc,char**argv){
  int N=atoi(argv[1]); const char*R="23456789TJQKA"; int ns[]={1,2,3,4,5,6,7,8};
  for(int hi=12;hi>=0;hi--)for(int lo=hi;lo>=0;lo--)for(int su=0;su<2;su++){
    if(hi==lo&&su)continue;
    int h0=hi*4,h1=lo*4+(su?0:1);
    int deck[50],nd=0;for(int c=0;c<52;c++)if(c!=h0&&c!=h1)deck[nd++]=c;
    printf("%c%c%s",R[hi],R[lo],hi==lo?"":(su?"s":"o"));
    for(int ni=0;ni<8;ni++){int n=ns[ni];double eq=0;int need=2*n+5;
      for(int it=0;it<N;it++){
        for(int i=0;i<need;i++){int j=i+rnd()%(nd-i);int t=deck[i];deck[i]=deck[j];deck[j]=t;}
        int c[7];c[0]=h0;c[1]=h1;for(int i=0;i<5;i++)c[2+i]=deck[2*n+i];
        int me=eval7(c),best=-1,cntb=0;
        for(int o=0;o<n;o++){c[0]=deck[2*o];c[1]=deck[2*o+1];int v=eval7(c);if(v>best){best=v;cntb=1;}else if(v==best)cntb++;}
        if(me>best)eq+=1;else if(me==best)eq+=1.0/(cntb+1);
      }
      printf(",%.4f",eq/N);}
    printf("\n");fflush(stdout);
  }
}
