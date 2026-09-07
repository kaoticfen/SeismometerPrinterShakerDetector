#pragma once
#include <Arduino.h>

// Served from flash rather than copied to RAM -- the radios need the heap.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Seismo</title>
<style>
:root{--bg:#111417;--fg:#e8eaed;--dim:#8b949e;--accent:#5ac8fa;--warn:#ff6b6b;}
*{box-sizing:border-box}
body{margin:0;padding:12px;background:var(--bg);color:var(--fg);
 font:14px/1.5 -apple-system,system-ui,sans-serif}
h1{font-size:16px;margin:0 0 10px;letter-spacing:.06em;text-transform:uppercase}
canvas{width:100%;height:220px;background:#0a0c0e;border:1px solid #262b31;
 border-radius:6px;display:block}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin:12px 0}
.cell{background:#181c20;border:1px solid #262b31;border-radius:6px;padding:8px 10px}
.k{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.05em}
.v{font-size:20px;font-variant-numeric:tabular-nums}
button{width:100%;padding:14px;font-size:15px;font-weight:600;color:#04202b;
 background:var(--accent);border:0;border-radius:6px}
button:disabled{opacity:.4}
#msg{min-height:20px;color:var(--dim);font-size:12px;margin-top:8px}
.bad{color:var(--warn)}
</style></head><body>
<h1>Seismograph</h1>
<canvas id="c"></canvas>
<div class="grid">
 <div class="cell"><div class="k">Peak</div><div class="v" id="pk">--</div></div>
 <div class="cell"><div class="k">RMS</div><div class="v" id="rms">--</div></div>
 <div class="cell"><div class="k">Dominant</div><div class="v" id="hz">--</div></div>
 <div class="cell"><div class="k">Full scale</div><div class="v" id="fs">--</div></div>
</div>
<button id="p">Print last window</button>
<div id="msg"></div>
<script>
const cv=document.getElementById('c'),cx=cv.getContext('2d');
const N=240; let trace=[], fs=100, link='';
function fit(){const r=cv.getBoundingClientRect(),d=devicePixelRatio||1;
 cv.width=r.width*d;cv.height=r.height*d;cx.setTransform(d,0,0,d,0,0);}
addEventListener('resize',fit);fit();

function draw(){
 const w=cv.clientWidth,h=cv.clientHeight,mid=h/2;
 cx.clearRect(0,0,w,h);
 // gridlines at 0 and +/- half scale
 cx.strokeStyle='#262b31';cx.lineWidth=1;cx.setLineDash([3,4]);
 [0.5,0,-0.5].forEach(f=>{const y=mid-f*mid;
  cx.beginPath();cx.moveTo(0,y+.5);cx.lineTo(w,y+.5);cx.stroke();});
 cx.setLineDash([]);
 if(!trace.length)return;
 // min/max envelope, same decimation the printer uses
 const dx=w/N;
 cx.strokeStyle='#5ac8fa';cx.lineWidth=Math.max(1,dx*0.8);
 for(let i=0;i<trace.length;i++){
  const b=trace[i],x=i*dx+dx/2;
  let lo=mid-(b[0]/fs)*mid, hi=mid-(b[1]/fs)*mid;
  // clipped bins go red so you can see to back the gain off
  const clip=Math.abs(b[0])>fs||Math.abs(b[1])>fs;
  cx.strokeStyle=clip?'#ff6b6b':'#5ac8fa';
  lo=Math.max(0,Math.min(h,lo));hi=Math.max(0,Math.min(h,hi));
  if(Math.abs(lo-hi)<1)hi=lo+1;
  cx.beginPath();cx.moveTo(x,lo);cx.lineTo(x,hi);cx.stroke();
 }
}

async function tick(){
 try{
  const r=await fetch('/api/wave?n='+N);
  const j=await r.json();
  trace=j.bins;fs=j.fullScaleMg||100;link=j.link;
  document.getElementById('pk').textContent=j.peakMg.toFixed(1)+' mg';
  document.getElementById('rms').textContent=j.rmsMg.toFixed(2)+' mg';
  document.getElementById('hz').textContent=j.domHz>0?j.domHz.toFixed(2)+' Hz':'--';
  document.getElementById('fs').textContent='±'+fs.toFixed(0)+' mg';
  draw();
 }catch(e){/* AP dropped a poll, next one will catch up */}
}
setInterval(tick,200);tick();

const btn=document.getElementById('p'),msg=document.getElementById('msg');
btn.onclick=async()=>{
 btn.disabled=true;msg.className='';msg.textContent='printing…';
 try{
  const r=await fetch('/api/print',{method:'POST'});
  const j=await r.json();
  msg.className=j.ok?'':'bad';
  msg.textContent=j.ok?'printed '+j.seconds.toFixed(1)+'s window':('failed: '+j.error);
 }catch(e){msg.className='bad';msg.textContent='request failed';}
 btn.disabled=false;
};
</script></body></html>
)HTML";
