/* the new synth: switching sounds under ringing notes, levels per sound, CPU, WAV previews */
#include "synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
static synth_t S;
static void wav(const char*fn, float*L, float*R, int n, int sr){FILE*f=fopen(fn,"wb");int ds=n*4;int hdr[11];
 fwrite("RIFF",1,4,f);int x=36+ds;fwrite(&x,4,1,f);fwrite("WAVEfmt ",1,8,f);x=16;fwrite(&x,4,1,f);short a=1,c=2;fwrite(&a,2,1,f);fwrite(&c,2,1,f);fwrite(&sr,4,1,f);x=sr*4;fwrite(&x,4,1,f);a=4;fwrite(&a,2,1,f);a=16;fwrite(&a,2,1,f);fwrite("data",1,4,f);fwrite(&ds,4,1,f);
 for(int i=0;i<n;i++){short l=(short)(L[i]*32767),r=(short)(R[i]*32767);fwrite(&l,2,1,f);fwrite(&r,2,1,f);}fclose(f);(void)hdr;}
int main(int argc,char**argv){
 const int sr=48000; int fails=0;
 /* 1: switch every sound to every other while a chord rings: never louder than the sound itself, never full scale */
 float l[128],r[128];
 for(int a=0;a<NUM_SOUNDS;a++) for(int b=0;b<NUM_SOUNDS;b++){
   synth_init(&S,sr); S.level=0.75f; S.space=0.3f; S.sound=a;
   int ch[5]={48,55,60,64,67}; for(int i=0;i<5;i++) synth_note_on(&S,ch[i],110);
   for(int k=0;k<150;k++) synth_render(&S,l,r,128);
   S.sound=b; float pk=0;
   for(int k=0;k<150;k++){ synth_render(&S,l,r,128); for(int i=0;i<128;i++){ if(!isfinite(l[i])) pk=99; if(fabsf(l[i])>pk)pk=fabsf(l[i]); if(fabsf(r[i])>pk)pk=fabsf(r[i]);} }
   for(int i=0;i<5;i++) synth_note_on(&S,ch[i]+2,110);   /* new notes in the new sound over the old ones */
   for(int k=0;k<150;k++){ synth_render(&S,l,r,128); for(int i=0;i<128;i++){ if(!isfinite(l[i])) pk=99; if(fabsf(l[i])>pk)pk=fabsf(l[i]);} }
   if(pk>0.95f){ printf("FAIL switch %d -> %d: peak %.3f\n",a,b,pk); fails++; }
 }
 printf("%s 256 sound switches under ringing chords\n", fails?"FAIL":"ok  ");
 /* 2: level and CPU per sound: a 5-note chord at velocity 100, 3 s, then release */
 static float L[48000*5], R[48000*5];
 for(int s=0;s<NUM_SOUNDS;s++){
   synth_init(&S,sr); S.level=0.75f; S.space=0.3f; S.sound=s; S.tone=0.5f;
   int ch[5]={48,55,60,64,71}; for(int i=0;i<5;i++) synth_note_on(&S,ch[i],100);
   int n=sr*5; clock_t t0=clock();
   for(int p=0;p<n;p+=128){ if(p==sr*3) for(int i=0;i<5;i++) synth_note_off(&S,ch[i]); synth_render(&S,L+p,R+p,128); }
   double cpu=(double)(clock()-t0)/CLOCKS_PER_SEC/5.0*100;
   double rms=0; float pk=0; for(int i=0;i<sr*2;i++){ rms+=L[i]*L[i]; if(fabsf(L[i])>pk)pk=fabsf(L[i]); } rms=sqrt(rms/(sr*2));
   int act=0; for(int k=0;k<SYN_VOICES;k++) act+=S.v[k].active;
   printf("%-15s rms %.3f (%5.1f dB) peak %.3f  cpu %.2f%%  voices left %d\n", SOUND_NAMES[s], rms, 20*log10(rms+1e-9), pk, cpu, act);
   if(argc>1){ char fn[256]; snprintf(fn,sizeof fn,"%s/%02d %s.wav",argv[1],s,SOUND_NAMES[s]); wav(fn,L,R,n,sr); }
 }
 return fails?1:0;
}
