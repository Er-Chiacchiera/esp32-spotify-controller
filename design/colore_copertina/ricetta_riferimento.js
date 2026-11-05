// Implementazione di riferimento della ricetta (SPEC_COLORE.md §1), in JavaScript.
// Serve solo a controllare i valori del §1.4: px = RGBA 300×300 (es. getImageData).
// ricetta(px, 300).bg = [R,G,B] di NP_BG; mix(c, k) = toni del §2.1.
// Ricetta del colore di fondo (riferimento per il firmware). px: Uint8ClampedArray RGBA 300x300.
function lin(c){c/=255;return c<=0.04045?c/12.92:Math.pow((c+0.055)/1.055,2.4)}
function Y(r,g,b){return 0.2126*lin(r)+0.7152*lin(g)+0.0722*lin(b)}
function hsl2rgb(h,s,l){const c=(1-Math.abs(2*l-1))*s,x=c*(1-Math.abs((h/60)%2-1)),m=l-c/2;let r,g,b;
 if(h<60)[r,g,b]=[c,x,0];else if(h<120)[r,g,b]=[x,c,0];else if(h<180)[r,g,b]=[0,c,x];else if(h<240)[r,g,b]=[0,x,c];else if(h<300)[r,g,b]=[x,0,c];else[r,g,b]=[c,0,x];
 return [Math.round((r+m)*255),Math.round((g+m)*255),Math.round((b+m)*255)]}
function rgb2hsl(r,g,b){r/=255;g/=255;b/=255;const mx=Math.max(r,g,b),mn=Math.min(r,g,b),l=(mx+mn)/2,d=mx-mn;let h=0,s=0;
 if(d>0){s=d/(1-Math.abs(2*l-1));if(mx===r)h=60*(((g-b)/d)%6);else if(mx===g)h=60*((b-r)/d+2);else h=60*((r-g)/d+4);if(h<0)h+=360}return [h,s,l]}
function hue(r,g,b){return rgb2hsl(r,g,b)[0]}
function to565(c){return [c[0]&0xF8,c[1]&0xFC,c[2]&0xF8]}
function ricetta(px,W){
 const cnt=new Array(36).fill(0),sr=new Array(36).fill(0),sg=new Array(36).fill(0),sb=new Array(36).fill(0);let n=0,viv=0;
 for(let y=3;y<W;y+=6)for(let x=3;x<W;x+=6){const i=(y*W+x)*4;
  // il firmware legge RGB565: simuliamo la perdita di bit
  const r=px[i]&0xF8,g=px[i+1]&0xFC,b=px[i+2]&0xF8;n++;
  const mx=Math.max(r,g,b),mn=Math.min(r,g,b),C=mx-mn;
  if(mx<40||C<24||C*100<mx*30)continue;
  const k=Math.min(35,Math.floor(hue(r,g,b)/10));cnt[k]++;sr[k]+=r;sg[k]+=g;sb[k]+=b;viv++}
 let H=0,S=0,best=-1,bs=-1;
 if(viv*100>=n*5){
  for(let k=0;k<36;k++){const s=cnt[(k+35)%36]+2*cnt[k]+cnt[(k+1)%36];if(s>bs){bs=s;best=k}}
  let R=0,G=0,B=0,N=0;for(const k of[(best+35)%36,best,(best+1)%36]){R+=sr[k];G+=sg[k];B+=sb[k];N+=cnt[k]}
  const h=rgb2hsl(R/N,G/N,B/N);H=h[0];S=Math.min(0.60,Math.max(0.30,h[1]))}
 let c;for(let L=30;L>=6;L--){c=hsl2rgb(H,S,L/100);if(Y(...c)<=0.032)break}
 return {bg:c,viv:Math.round(viv*100/n),H:Math.round(H),S:+S.toFixed(2)}}
function mix(c,k){return (c.map(v=>Math.round(v+(255-v)*k/256)))}
function contrast(a,b){const y1=Y(...a),y2=Y(...b);return (Math.max(y1,y2)+0.05)/(Math.min(y1,y2)+0.05)}
function hex(c){return '#'+c.map(v=>v.toString(16).padStart(2,'0')).join('').toUpperCase()}
