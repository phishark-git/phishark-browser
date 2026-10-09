// SPDX-License-Identifier: Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import {createFixtureServer} from '../scripts/device-fixtures.mjs';
import {BrowserClient} from '../shared/security-contract/client.mjs';
import {profiles} from '../shared/security-contract/policy.mjs';

test('local HTTP fixtures exercise real transport, verdicts, privacy and stalled-body cancellation',async t=>{
  const fixture=createFixtureServer({stallMs:1000});
  await new Promise(resolve=>fixture.server.listen(0,'127.0.0.1',resolve));
  t.after(()=>fixture.close());
  const baseURL=`http://127.0.0.1:${fixture.server.address().port}`;
  const client=new BrowserClient({baseURL,apiKey:'fixture-only',budgets:{preflight:150,deep:150}});
  const evidence={response:{html:'<html><body>sanitized fixture</body></html>'}};
  for(const [scenario,profile,policy] of [
    ['safe',profiles.preflight,'safe'],['preflight-block',profiles.preflight,'blocked'],
    ['preflight-warning',profiles.preflight,'warning'],['preflight-warning',profiles.deep,'safe'],
    ['deep-warning',profiles.deep,'warning'],['deep-block',profiles.deep,'blocked'],
    ['prompt-suspicious',profiles.deep,'blocked'],['prompt-malicious',profiles.deep,'blocked'],
    ['degraded',profiles.deep,'unverified'],['negative',profiles.preflight,'unverified'],
    ['malformed',profiles.preflight,'unverified'],['temporary',profiles.preflight,'unverified'],
    ['auth',profiles.preflight,'service_error'],['quota',profiles.preflight,'service_error'],
    ['configuration',profiles.preflight,'service_error'],['capacity',profiles.preflight,'safe'],
    ['stall',profiles.preflight,'unverified']]){
    const result=await client.scan(profile,`${baseURL}/pages/${scenario}`,profile===profiles.deep?{consent:true,evidence}:{});
    assert.equal(result.policy,policy,scenario);
  }
  assert.equal(fixture.stats.capacityRetries,1);
  const before=fixture.stats.deep;
  await assert.rejects(client.scan(profiles.deep,`${baseURL}/pages/safe`,{privateMode:true,consent:true,evidence}));
  assert.equal(fixture.stats.deep,before);
  const leaked=await client.scan(profiles.deep,`${baseURL}/pages/capture`,{consent:true,evidence:{response:{html:'<input value="fixture-private-password">'}}});
  assert.equal(leaked.policy,'service_error');assert.equal(fixture.stats.privacyRejected,1);
  const cookieLeak=await fetch(baseURL+'/api/v1/browser/preflight',{method:'POST',headers:{'Content-Type':'application/json','X-API-Key':'fixture-only',Cookie:'fixture-session=fixture-private-cookie'},body:JSON.stringify({target:baseURL+'/pages/safe'})});
  assert.equal(cookieLeak.status,400);assert.equal(fixture.stats.privacyRejected,2);
  const redirect=await fetch(baseURL+'/redirect/2');
  assert.equal(new URL(redirect.url).pathname,'/pages/safe');
  const capture=await(await fetch(baseURL+'/pages/capture')).text();
  assert.ok(capture.includes('attachShadow'));assert.ok(capture.includes('iframe'));assert.ok(capture.includes('contenteditable'));
  assert.equal((await client.scan(profiles.preflight,`${baseURL}/pages/safe`,{privateMode:true})).policy,'safe');
  const controller=new AbortController();const pending=client.scan(profiles.preflight,`${baseURL}/pages/stall`,{signal:controller.signal});
  controller.abort();assert.equal((await pending).cancelled,true);
});
