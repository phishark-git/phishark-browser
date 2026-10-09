// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
const vectors=[];
for(const [profile,block] of [['preflight',86],['browser_deep_scan',61]]){
  for(const score of [-1,0,10,30,31,60,61,85,86,100,101])vectors.push({name:`${profile} score ${score}`,profile,response:{scan_profile:profile,verdict:'unknown',risk_calculation:{risk_score:score}},expected:score<0||score>100?'unverified':score>=block?'blocked':score>=31?'warning':'safe'});
  for(const verdict of ['unsafe','malicious','blocked','phishing','dangerous'])vectors.push({name:`${profile} definitive ${verdict}`,profile,response:{scan_profile:profile,verdict,risk_score:10},expected:'blocked'});
}
for(const reason of ['gatekeeper_malicious:blocklist','prompt_injection_detected','prompt_injection_suspected'])vectors.push({name:reason,profile:'browser_deep_scan',response:{scan_profile:'browser_deep_scan',risk_score:0,short_circuit_reason:reason},expected:'blocked'});
vectors.push({name:'mismatched profile',profile:'preflight',response:{scan_profile:'browser_deep_scan',risk_score:0},expected:'unverified'},{name:'missing score',profile:'preflight',response:{scan_profile:'preflight'},expected:'unverified'},{name:'numeric string rejected',profile:'preflight',response:{scan_profile:'preflight',risk_score:'0'},expected:'unverified'});
vectors.push({name:'definitive threat survives missing score',profile:'preflight',response:{scan_profile:'preflight',verdict:'malicious'},expected:'blocked'}, {name:'failed is not safe',profile:'preflight',response:{scan_profile:'preflight',status:'failed',risk_score:0},expected:'unverified'});
vectors.push({name:'degraded prompt service cannot be declared safe',profile:'browser_deep_scan',response:{scan_profile:'browser_deep_scan',risk_score:0,analysis_degraded:true,incomplete_modules:['prompt-injection-analysis-service']},expected:'unverified'});
vectors.push({name:'normalized definitive verdict',profile:'preflight',response:{scan_profile:'preflight',risk_score:0,verdict:' Malicious '},expected:'blocked'});
const trusted={scan_profile:'preflight',status:'completed',risk_score:0,short_circuit_reason:'gatekeeper_benign:whitelist'};
for(const [name,changes,expected,skipDeep] of [
  ['whitelist',{},'safe',true],
  ['manual allow',{short_circuit_reason:' Gatekeeper_Benign:manual_allow '},'safe',true],
  ['unknown low score',{short_circuit_reason:''},'safe',false],
  ['benign verdict alone',{short_circuit_reason:'',verdict:'benign'},'safe',false],
  ['whitelist degraded',{analysis_degraded:true},'unverified',false],
  ['whitelist missing score',{risk_score:null},'unverified',false],
  ['whitelist without numeric analysis',{risk_score:null,verdict:'benign'},'safe',true],
  ['whitelist invalid numeric type',{risk_score:'0',verdict:'benign'},'unverified',false],
  ['whitelist negative score',{risk_score:-1},'unverified',false],
  ['whitelist missing completion',{status:null},'safe',false],
  ['whitelist failed',{status:'failed'},'unverified',false],
  ['whitelist conflicting threat',{verdict:'malicious'},'blocked',false],
  ['whitelist conflicting warning',{risk_score:31},'warning',false],
  ['whitelist conflicting block',{risk_score:86},'blocked',false],
  ['deep cannot grant whitelist bypass',{scan_profile:'browser_deep_scan'},'unverified',false],
  ['blacklist low score',{short_circuit_reason:'gatekeeper_malicious:blacklist'},'blocked',false],
])vectors.push({name,profile:'preflight',response:{...trusted,...changes},expected,skip_deep:skipDeep});
fs.mkdirSync(new URL('../shared/test-vectors/',import.meta.url),{recursive:true});
fs.writeFileSync(new URL('../shared/test-vectors/decisions.json',import.meta.url),JSON.stringify(vectors,null,2)+'\n');
