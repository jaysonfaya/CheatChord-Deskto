#include "harmony.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void show(int key,int scale,int style,float sp,const chord_t*prev,int n){
  hctx_t h={key,scale,style,sp,prev,NULL,NULL,NULL};
  unsigned char seen[HARMONY_GROUPS]; memset(seen,0,sizeof seen); uint32_t r=12345;
  for(int roll=0;roll<2;roll++){
    chord_t pool[8]; int k=harmony_pool(&h,pool,8,seen,&r,NULL,0);
    printf("  %s: ", roll?"reroll":"pool  ");
    for(int i=0;i<k;i++){char nm[32],ro[32];chord_name_key(&pool[i],key,scale,nm,32);harmony_roman(key,scale,&pool[i],ro,32);printf("%-10s(%s) ",nm,ro);}
    printf("\n");
  }
}
int main(int argc,char**argv){
  float sp=argc>1?atof(argv[1]):0.5f;
  for(int st=0;st<NUM_STYLES;st++){
    int scale = (st==3||st==8||st==10)?1:0;
    printf("== %s (%s) spice %.2f\n",STYLE_NAMES[st],SCALE_NAMES[scale],sp);
    show(0,scale,st,sp,NULL,8);
    chord_t p={scale?0:5, scale?Q_M7:Q_MAJ7,-1}; char nm[32]; chord_name(&p,0,nm,32); printf(" after %s\n",nm);
    show(0,scale,st,sp,&p,8);
    uint32_t r=st*77+1; chord_t g[8]; harmony_generate(0,scale,st,0,0,NULL,sp,8,g,&r);
    printf("  generate: "); for(int i=0;i<8;i++){chord_name_key(&g[i],0,scale,nm,32);printf("%s  ",nm);} printf("\n");
  }
}
