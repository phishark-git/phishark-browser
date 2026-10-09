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
fs.mkdirSync(new URL('../shared/test-vectors/',import.meta.url),{recursive:true});
fs.writeFileSync(new URL('../shared/test-vectors/decisions.json',import.meta.url),JSON.stringify(vectors,null,2)+'\n');
