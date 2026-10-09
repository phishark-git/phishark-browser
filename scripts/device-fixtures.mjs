// SPDX-License-Identifier: Apache-2.0
// Synthetic loopback-only acceptance fixtures. Never call a production API.
import http from 'node:http';
import {createHash,randomUUID} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {auditPNG} from './fixture-png-audit.mjs';
import fs from 'node:fs';

export const scenarios=['safe','preflight-warning','preflight-block','deep-warning','deep-block',
  'prompt-suspicious','prompt-malicious','degraded','negative','malformed','temporary',
  'auth','quota','configuration','capacity','stall','capture','popup','same-document',
  'duplicate-history','duplicate-history-slow','handoff-slow','whitelist','blacklist',
  'screenshot','screenshot-scrolled','screenshot-zoomed','screenshot-moving'];

function page(name) {
  if(name.startsWith('screenshot'))return screenshotPage(name);
  const links=scenarios.map(x=>`<li><a href="/pages/${x}">${x}</a></li>`).join('');
  return `<!doctype html><html><head><meta name="viewport" content="width=device-width"><title>PhiShark fixture: ${name}</title></head>
<body><h1>${name}</h1><p>Synthetic local acceptance fixture. Use API key fixture-only.</p>
<p><a href="/redirect/2">Two-hop redirect</a> · <a href="/pages/safe" target="_blank">New tab</a> · <a href="#section">Same-document anchor</a></p>
<button id="popup">Popup</button><button id="history">Same-document history state</button>
<button id="history-block">Same-document blocked URL</button>
<form><label>Email <input type="email" value="fixture-private-email@example.invalid"></label>
<label>Password <input type="password" value="fixture-private-password"></label>
<label>Text <input value="fixture-private-text"></label><textarea>fixture-private-textarea</textarea>
<select><option value="fixture-private-selection">Fixture choice</option></select><input type="file"></form>
<div contenteditable="true">fixture-private-editable</div><div id="shadow"></div>
<iframe src="/frame" title="Masking fixture frame"></iframe>
<p><a href="/download" download="fixture.txt">Download fixture</a></p><ul>${links}</ul>
<div style="height:900px"></div><h2 id="section">Scrolled masking area</h2><input value="fixture-private-scrolled">
<script>
document.getElementById('popup').onclick=()=>window.open('/pages/deep-block','fixture-popup');
document.getElementById('history').onclick=()=>history.pushState({},'',location.pathname+'?sameDocument=fixture');
document.getElementById('history-block').onclick=()=>history.pushState({},'','/pages/preflight-block?history=fixture');
document.getElementById('shadow').attachShadow({mode:'open'}).innerHTML='<label>Shadow field <input value="fixture-private-shadow"></label>';
if (${JSON.stringify(name)}.startsWith('duplicate-history')) {
  // Change evidence between events: a body-hash cache must not hide duplicates.
  for (let i=1;i<=5;i++) setTimeout(()=>{
    document.querySelector('h1').textContent='duplicate-history event '+i;
    history.replaceState({event:i},'',location.pathname+location.search+'#event'+i);
  }, i*800);
}
</script></body></html>`;
}

function screenshotPage(name) {
  return `<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=${name==='screenshot-zoomed'?1.5:1}"><title>PhiShark fixture: ${name}</title>
<style>body{margin:8px}input,textarea,select,[contenteditable],[role=textbox]{background:#ff00ff;color:#00ffff;width:240px;font-size:16px}label{display:block;margin:6px}iframe{width:280px;height:60px}#brand{background:#0000ff;color:white;padding:12px}</style></head>
<body><div style="height:${name==='screenshot-scrolled'?900:0}px"></div><div id="brand">PhiShark visual evidence fixture</div>
<label>Email <input value="fixture-private-email"></label><label>Password <input type="password" value="fixture-private-password"></label>
<textarea>fixture-private-textarea</textarea><select><option>fixture-private-select</option></select>
<div contenteditable="true">fixture-private-editable</div><div role="textbox">fixture-private-role</div>
<div id="open"></div><div id="closed"></div><custom-field></custom-field>
<iframe src="/frame-color" title="Sensitive frame"></iframe><p>Public visual text must remain visible.</p>
<script>
for(const [id,mode] of [['open','open'],['closed','closed']])document.getElementById(id).attachShadow({mode}).innerHTML='<input style="background:#ff00ff;color:#00ffff;width:240px;height:24px" value="fixture-private-shadow">';
document.querySelector('custom-field').attachShadow({mode:'closed'}).innerHTML='<div contenteditable style="background:#ff00ff;color:#00ffff;width:240px">fixture-private-custom</div>';
${name==='screenshot-scrolled'?'scrollTo(0,900);':''}
${name==='screenshot-moving'?"setInterval(()=>{document.querySelector('input').style.transform='translateX('+Math.random()*60+'px)'},5);":''}
</script></body></html>`;
}

export function createFixtureServer({stallMs=30000}={}) {
  const stats={preflight:0,deep:0,privacyRejected:0,capacityRetries:0,
    pageGets:Object.fromEntries(scenarios.map(name=>[name,0])),redirectGets:0,
    redirectPreflights:0,
    deepEvidence:{},
    requests:Object.fromEntries(scenarios.map(name=>[name,{preflight:0,deep:0}]))};
  const capacitySeen=new Set();
  const send=(res,status,body)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(body));};
  const server=http.createServer(async(req,res)=>{
    const route=new URL(req.url,'http://127.0.0.1');
    if(req.method==='GET') {
      if(route.pathname==='/stats')return send(res,200,stats);
      if(route.pathname==='/download'){res.writeHead(200,{'Content-Type':'text/plain','Content-Disposition':'attachment; filename="fixture.txt"'});return res.end('Synthetic download fixture\n');}
      if(route.pathname==='/redirect/whitelist'){
        stats.redirectGets++;
        res.writeHead(302,{Location:'/pages/deep-block?from=whitelist'});return res.end();
      }
      if(/^\/redirect\/[12]$/.test(route.pathname)){
        stats.redirectGets++;
        res.writeHead(302,{Location:route.pathname.endsWith('/2')?'/redirect/1':'/pages/safe?redirect=fixture'});return res.end();
      }
      if(route.pathname==='/frame'){res.writeHead(200,{'Content-Type':'text/html'});return res.end('<label>Frame input <input value="fixture-private-frame"></label>');}
      if(route.pathname==='/frame-color'){res.writeHead(200,{'Content-Type':'text/html'});return res.end('<body style="margin:0;background:#ff00ff"><input value="fixture-private-frame"></body>');}
      const name=route.pathname.split('/')[2]||'safe';
      if(route.pathname!=='/'&&!scenarios.includes(name)){res.writeHead(404);return res.end();}
      stats.pageGets[name]++;
      if(name==='handoff-slow') {
        const timer=setTimeout(()=>{
          res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});
          res.end(page(name));
        },1500);
        res.once('close',()=>clearTimeout(timer));return;
      }
      res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Set-Cookie':'fixture-session=fixture-private-cookie; HttpOnly; SameSite=Lax','Cache-Control':'no-store'});
      return res.end(page(name));
    }
    const deep=route.pathname==='/api/v1/browser/deep';
    if(req.method!=='POST'||(!deep&&route.pathname!=='/api/v1/browser/preflight')||route.search)return send(res,404,{code:'FIXTURE_ROUTE'});
    let payload;
    try {
      const parts=[];let bytes=0;
      for await(const part of req){bytes+=part.length;if(bytes>6*1024*1024)throw new Error('size');parts.push(part);}
      payload=JSON.parse(Buffer.concat(parts).toString('utf8'));
    }catch{return send(res,400,{code:'FIXTURE_INVALID_BODY'});}
    if(req.headers['x-api-key']!=='fixture-only')return send(res,401,{code:'INVALID_API_KEY'});
    let target;
    try{target=new URL(payload.target);if(!['127.0.0.1','localhost','fixture.invalid'].includes(target.hostname))throw new Error('host');}
    catch{return send(res,400,{code:'FIXTURE_TARGET_ONLY'});}
    const serialized=JSON.stringify(payload.web_evidence??{});
    const forbidden=req.headers.cookie||req.headers.authorization||
      /"(?:cookie|cookies|authorization)"\s*:/i.test(serialized)||serialized.includes('fixture-private-');
    if(forbidden||(!deep&&payload.web_evidence!==undefined)){
      stats.privacyRejected++;return send(res,400,{code:'FIXTURE_PRIVACY_FAILURE'});
    }
    if(deep&&!payload.web_evidence?.response?.html&&!payload.web_evidence?.response?.screenshot)return send(res,400,{code:'FIXTURE_EVIDENCE_REQUIRED'});
    stats[deep?'deep':'preflight']++;
    const redirectTarget=/^\/redirect\/[12]$/.test(target.pathname);
    const scenario=redirectTarget?'safe':target.pathname.split('/')[2]||'safe';
    if(redirectTarget&&!deep)stats.redirectPreflights++;
    if(!scenarios.includes(scenario))return send(res,400,{code:'FIXTURE_SCENARIO'});
    stats.requests[scenario][deep?'deep':'preflight']++;
    if(deep){
      // Only synthetic-fixture metadata, never captured HTML/pixels or URL logs.
      const response=payload.web_evidence.response;
      stats.deepEvidence[scenario]={
        htmlBytes:Buffer.byteLength(response.html??''),
        screenshotBytes:Buffer.byteLength(response.screenshot??''),
        hasHTML:typeof response.html==='string'&&response.html.length>0,
        hasScreenshot:typeof response.screenshot==='string'&&response.screenshot.length>0,
        capturedTitleMatches:typeof response.html==='string'&&response.html.includes(`<title>PhiShark fixture: ${scenario}</title>`),
        responseURLMatchesTarget:response.url===payload.target,
        coverage:payload.web_evidence.capture_coverage??null,
        containsScript:/<script\b/i.test(response.html??''),
        containsFrame:/<(?:iframe|frame)\b/i.test(response.html??''),
      };
      if(response.fixture_capture_failure_step!==undefined)
        stats.deepEvidence[scenario].captureFailureStep=response.fixture_capture_failure_step;
      if(response.fixture_capture_viewport_before)stats.deepEvidence[scenario].captureViewport={
        before:response.fixture_capture_viewport_before,after:response.fixture_capture_viewport_after,
        width:response.fixture_capture_width,height:response.fixture_capture_height};
      if(response.screenshot){
        try{
          stats.deepEvidence[scenario].png=auditPNG(response.screenshot,response.fixture_mask_regions??[]);
          if(scenario.startsWith('screenshot')&&stats.deepEvidence[scenario].png.magentaPixels>0){
            stats.privacyRejected++;return send(res,400,{code:'FIXTURE_UNMASKED_CANARY'});
          }
          // Opt-in, synthetic loopback fixtures only. Never save production evidence.
          if(process.env.PHISHARK_FIXTURE_PNG_DIR&&scenario.startsWith('screenshot')){
            fs.mkdirSync(process.env.PHISHARK_FIXTURE_PNG_DIR,{recursive:true});
            fs.writeFileSync(`${process.env.PHISHARK_FIXTURE_PNG_DIR}/${scenario}.png`,Buffer.from(response.screenshot,'base64'));
          }
        }catch{stats.privacyRejected++;return send(res,400,{code:'FIXTURE_PNG_FAILURE'});}
      }
    }
    if(scenario==='auth')return send(res,401,{code:'INVALID_API_KEY'});
    if(scenario==='quota')return send(res,429,{code:'QUOTA_EXCEEDED'});
    if(scenario==='configuration')return send(res,503,{code:'BROWSER_NOT_CONFIGURED'});
    if(scenario==='temporary')return send(res,503,{code:'SERVICE_UNAVAILABLE'});
    if(scenario==='malformed'){res.writeHead(200,{'Content-Type':'application/json'});return res.end('{');}
    if(scenario==='capacity'){
      // Keep only an in-memory digest, never a target or evidence log.
      const key=createHash('sha256').update(payload.target+(deep?'deep':'preflight')).digest('hex');
      if(!capacitySeen.has(key)){
        if(capacitySeen.size>=1000)capacitySeen.delete(capacitySeen.values().next().value);
        capacitySeen.add(key);stats.capacityRetries++;res.setHeader('Retry-After','0');return send(res,429,{code:'PROFILE_CAPACITY'});
      }
    }
    const result={success:true,data:{scan_id:randomUUID(),target:payload.target,status:'completed',
      scan_profile:deep?'browser_deep_scan':'preflight',analysis_degraded:scenario==='degraded',
      risk_calculation:{risk_score:scenario==='negative'?-1:
        scenario==='preflight-block'&&!deep?86:
        scenario==='preflight-warning'&&!deep?31:
        scenario==='deep-warning'&&deep?31:
        ['deep-block','whitelist'].includes(scenario)&&deep?61:0}}};
    if(!deep&&scenario==='whitelist'){
      result.data.verdict='benign';result.data.short_circuit_reason='gatekeeper_benign:whitelist';
      delete result.data.risk_calculation; // Server short circuits may have no numeric analysis.
    }
    if(!deep&&scenario==='blacklist'){
      result.data.verdict='malicious';result.data.short_circuit_reason='gatekeeper_malicious:blacklist';
    }
    if(deep&&scenario.startsWith('prompt-')){
      result.data.verdict='blocked';result.data.risk_calculation.risk_score=100;
      result.data.short_circuit_reason=scenario==='prompt-suspicious'?'prompt_injection_suspected':'prompt_injection_detected';
    }
    if(scenario==='stall'){
      res.writeHead(200,{'Content-Type':'application/json'});res.flushHeaders();
      const timer=setTimeout(()=>res.end(JSON.stringify(result)),stallMs);
      res.once('close',()=>clearTimeout(timer));return;
    }
    if(scenario==='duplicate-history-slow'&&deep){
      const timer=setTimeout(()=>send(res,200,result),3000);
      res.once('close',()=>clearTimeout(timer));return;
    }
    if(scenario==='handoff-slow'){
      const timer=setTimeout(()=>send(res,200,result),deep?3000:1000);
      res.once('close',()=>clearTimeout(timer));return;
    }
    return send(res,200,result);
  });
  return {server,stats,close:()=>new Promise(resolve=>{server.close(resolve);server.closeAllConnections();capacitySeen.clear();})};
}

if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  const fixture=createFixtureServer();
  fixture.server.listen(8765,'127.0.0.1',()=>process.stdout.write('Local fixtures: http://127.0.0.1:8765 (API key: fixture-only)\n'));
  for(const signal of ['SIGINT','SIGTERM'])process.once(signal,async()=>{await fixture.close();process.exit(0);});
}
