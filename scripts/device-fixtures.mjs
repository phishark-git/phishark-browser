// SPDX-License-Identifier: Apache-2.0
// Synthetic loopback-only acceptance fixtures. Never call a production API.
import http from 'node:http';
import {createHash,randomUUID} from 'node:crypto';
import {pathToFileURL} from 'node:url';

export const scenarios=['safe','preflight-warning','preflight-block','deep-warning','deep-block',
  'prompt-suspicious','prompt-malicious','degraded','negative','malformed','temporary',
  'auth','quota','configuration','capacity','stall','capture','popup','same-document',
  'duplicate-history','duplicate-history-slow'];

function page(name) {
  const links=scenarios.map(x=>`<li><a href="/pages/${x}">${x}</a></li>`).join('');
  return `<!doctype html><html><head><meta name="viewport" content="width=device-width"><title>PhiShark fixture: ${name}</title></head>
<body><h1>${name}</h1><p>Synthetic local acceptance fixture. Use API key fixture-only.</p>
<p><a href="/redirect/2">Two-hop redirect</a> · <a href="/pages/safe" target="_blank">New tab</a> · <a href="#section">Same-document anchor</a></p>
<button id="popup">Popup</button><button id="history">Same-document history state</button>
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

export function createFixtureServer({stallMs=30000}={}) {
  const stats={preflight:0,deep:0,privacyRejected:0,capacityRetries:0,
    pageGets:Object.fromEntries(scenarios.map(name=>[name,0])),redirectGets:0,
    redirectPreflights:0,
    requests:Object.fromEntries(scenarios.map(name=>[name,{preflight:0,deep:0}]))};
  const capacitySeen=new Set();
  const send=(res,status,body)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(body));};
  const server=http.createServer(async(req,res)=>{
    const route=new URL(req.url,'http://127.0.0.1');
    if(req.method==='GET') {
      if(route.pathname==='/stats')return send(res,200,stats);
      if(route.pathname==='/download'){res.writeHead(200,{'Content-Type':'text/plain','Content-Disposition':'attachment; filename="fixture.txt"'});return res.end('Synthetic download fixture\n');}
      if(/^\/redirect\/[12]$/.test(route.pathname)){
        stats.redirectGets++;
        res.writeHead(302,{Location:route.pathname.endsWith('/2')?'/redirect/1':'/pages/safe?redirect=fixture'});return res.end();
      }
      if(route.pathname==='/frame'){res.writeHead(200,{'Content-Type':'text/html'});return res.end('<label>Frame input <input value="fixture-private-frame"></label>');}
      const name=route.pathname.split('/')[2]||'safe';
      if(route.pathname!=='/'&&!scenarios.includes(name)){res.writeHead(404);return res.end();}
      stats.pageGets[name]++;
      res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Set-Cookie':'fixture-session=fixture-private-cookie; HttpOnly; SameSite=Lax','Cache-Control':'no-store'});
      return res.end(page(name));
    }
    const deep=route.pathname==='/api/v1/browser/deep';
    if(req.method!=='POST'||(!deep&&route.pathname!=='/api/v1/browser/preflight')||route.search)return send(res,404,{code:'FIXTURE_ROUTE'});
    let payload;
    try {
      const parts=[];let bytes=0;
      for await(const part of req){bytes+=part.length;if(bytes>1024*1024)throw new Error('size');parts.push(part);}
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
        scenario==='deep-block'&&deep?61:0}}};
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
    return send(res,200,result);
  });
  return {server,stats,close:()=>new Promise(resolve=>{server.close(resolve);server.closeAllConnections();capacitySeen.clear();})};
}

if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  const fixture=createFixtureServer();
  fixture.server.listen(8765,'127.0.0.1',()=>process.stdout.write('Local fixtures: http://127.0.0.1:8765 (API key: fixture-only)\n'));
  for(const signal of ['SIGINT','SIGTERM'])process.once(signal,async()=>{await fixture.close();process.exit(0);});
}
