import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {decide,canonicalURL,publicHostname,profiles,errorPolicy,trustedPreflight} from '../shared/security-contract/policy.mjs';
import {VerdictCache} from '../shared/security-contract/cache.mjs';
import {NavigationSession} from '../shared/security-contract/navigation.mjs';
import {BrowserClient} from '../shared/security-contract/client.mjs';
test('source-derived verdict vectors',()=>{
  for(const v of JSON.parse(fs.readFileSync(new URL('../shared/test-vectors/decisions.json',import.meta.url)))){
    assert.equal(decide(v.profile,v.response),v.expected,v.name);
    assert.equal(trustedPreflight(v.response),v.skip_deep??false,v.name);
  }
});
test('allowlist capture bypass stays within its navigation; unknown, stale and blocked results cannot inherit it',()=>{
  const tab=new NavigationSession({consent:true});
  const allow={scan_profile:profiles.preflight,status:'completed',risk_score:0,short_circuit_reason:'gatekeeper_benign:whitelist'};
  const first=tab.begin('https://allowed.example/');
  tab.apply(first,profiles.preflight,allow);assert.equal(tab.canCapture(),false);
  tab.unavailable(first);assert.equal(tab.canCapture(),false);
  const redirect=tab.begin('https://unknown.example/');
  assert.equal(tab.apply(first,profiles.preflight,allow),false);
  tab.apply(redirect,profiles.preflight,{scan_profile:profiles.preflight,risk_score:0});
  assert.equal(tab.canCapture(),true);
  tab.apply(redirect,profiles.deep,{scan_profile:profiles.deep,risk_score:61});
  assert.equal(tab.canCapture(),false);
  assert.equal(tab.apply(redirect,profiles.preflight,allow),false);
  tab.close();const next=tab.begin('https://new.example/');
  tab.apply(next,profiles.preflight,{scan_profile:profiles.preflight,risk_score:0});
  assert.equal(tab.canCapture(),true);
});
test('normalization preserves path/query, strips fragment and handles IDN/default ports',()=>{
  assert.equal(canonicalURL('https://EXAMPLE.com:443/a?token=fixture#fragment'),'https://example.com/a?token=fixture');
  assert.equal(canonicalURL('https://bücher.example/'),'https://xn--bcher-kva.example/');
  for(const url of ['javascript:alert(1)','file:///etc/passwd','https://user:password@example.com/'])assert.throws(()=>canonicalURL(url));
  for(const host of ['127.0.0.1','10.0.0.1','169.254.169.254','::1','fd00::1','example.local'])assert.equal(publicHostname(host),false);
});
test('private capture, stale decisions and reopening blocked pages are rejected',()=>{
  const tab=new NavigationSession({privateMode:true,consent:true});
  const old=tab.begin('https://first.example/'),now=tab.begin('https://second.example/');
  assert.equal(tab.canCapture(),false);assert.equal(tab.apply(old,profiles.preflight,{scan_profile:profiles.preflight,risk_score:100}),false);
  tab.apply(now,profiles.preflight,{scan_profile:profiles.preflight,risk_score:100});
  assert.equal(tab.apply(now,profiles.deep,{scan_profile:profiles.deep,risk_score:0}),false);
  assert.equal(tab.state.policy,'blocked');tab.close();assert.equal(tab.state.url,'');
});
test('cache deduplicates, expires and cannot repopulate after private-session teardown',async()=>{
  let clock=0,calls=0;const cache=new VerdictCache(100,2,()=>clock),factory=async()=>({policy:'safe',checkedAt:++calls});
  await Promise.all([cache.check('a',factory),cache.check('a',factory)]);assert.equal(calls,1);
  clock=100;await cache.check('a',factory);assert.equal(calls,2);
  let finish;const p=cache.check('b',()=>new Promise(r=>finish=r));await Promise.resolve();cache.clear();finish({policy:'safe'});await p;assert.equal(cache.get('b'),undefined);
  await cache.check('c',async()=>({policy:'unverified'}));assert.equal(cache.get('c'),undefined);
});
test('API uses only ephemeral routes; preflight and private mode cannot send content',async()=>{
  const requests=[],client=new BrowserClient({baseURL:'https://api.example',apiKey:'test-only',fetchImpl:async(url,init)=>{requests.push({url,init});return new Response(JSON.stringify({data:{scan_profile:profiles.preflight,risk_score:0}}));}});
  assert.equal((await client.scan(profiles.preflight,'https://example.com/')).policy,'safe');
  assert.equal(requests[0].url,'https://api.example/api/v1/browser/preflight');assert.deepEqual(JSON.parse(requests[0].init.body),{target:'https://example.com/'});assert.equal(requests[0].init.redirect,'error');
  await assert.rejects(client.scan(profiles.deep,'https://example.com/',{privateMode:true,consent:true}));
  await assert.rejects(client.scan(profiles.preflight,'https://example.com/',{evidence:{response:{html:'fixture'}}}));
});
test('deadline includes response body; capacity retries once and failures stay unverified',async()=>{
  let calls=0;const client=new BrowserClient({baseURL:'https://api.example',apiKey:'test-only',budgets:{preflight:20,deep:20},fetchImpl:async(url,{signal})=>{calls++;return {ok:true,status:200,text:()=>new Promise((resolve,reject)=>signal.addEventListener('abort',()=>reject(new Error('cancelled')),{once:true}))};}});
  assert.equal((await client.scan(profiles.preflight,'https://example.com/')).policy,'unverified');assert.equal(calls,1);
  calls=0;const limited=new BrowserClient({baseURL:'https://api.example',apiKey:'test-only',fetchImpl:async()=>{calls++;return new Response('',{status:429,headers:{'retry-after':'0'}});}});
  assert.equal((await limited.scan(profiles.preflight,'https://example.com/')).policy,'unverified');assert.equal(calls,2);
  for(const code of [0,429,500,503])assert.equal(errorPolicy(code),'unverified');for(const code of [400,401,402,403,404])assert.equal(errorPolicy(code),'service_error');
});
test('quota and disabled configuration are service errors, not capacity retries',async()=>{
 for(const [status,code] of [[429,'QUOTA_EXCEEDED'],[429,'LIMIT_EXCEEDED'],[503,'BROWSER_NOT_CONFIGURED'],[402,'INSUFFICIENT_CREDITS']]){
  let calls=0;
  const client=new BrowserClient({baseURL:'https://api.example',apiKey:'fixture',fetchImpl:async()=>{calls++;return new Response(JSON.stringify({code}),{status});}});
  assert.equal((await client.scan(profiles.preflight,'https://example.com/')).policy,'service_error');assert.equal(calls,1);
 }
 assert.equal(errorPolicy(501),'service_error');
});
