// SPDX-License-Identifier: Apache-2.0
const definitive = new Set(['unsafe','malicious','blocked','phishing','dangerous']);
export const profiles = Object.freeze({preflight:'preflight',deep:'browser_deep_scan'});
export function decide(profile,data) {
  if(!Object.values(profiles).includes(profile)) throw new TypeError('Unknown profile');
  if(!data || data.scan_profile!==profile) return 'unverified';
  const verdict=String(data.verdict??'').trim().toLowerCase(), reason=String(data.short_circuit_reason??'').trim().toLowerCase();
  if(definitive.has(verdict)||reason.startsWith('gatekeeper_malicious:')||['prompt_injection_detected','prompt_injection_suspected'].includes(reason))return 'blocked';
  if(data.status&&data.status!=='completed')return 'unverified';
  const score=data.risk_calculation?.risk_score??data.risk_score;
  if(profile===profiles.preflight && data.status==='completed' && data.analysis_degraded!==true
    && score==null && ['benign','safe','allowed'].includes(verdict)
    && reason.startsWith('gatekeeper_benign:'))return 'safe';
  if(typeof score!=='number'||!Number.isFinite(score)||score<0||score>100) return 'unverified';
  if(score>=(profile===profiles.preflight?86:61))return 'blocked';
  if(data.analysis_degraded===true)return 'unverified';
  return score>=31?'warning':'safe';
}
export function trustedPreflight(data) {
  return data?.status==='completed' && decide(profiles.preflight,data)==='safe'
    && String(data?.short_circuit_reason??'').trim().toLowerCase().startsWith('gatekeeper_benign:');
}
export function errorPolicy(status) {return status===0||status===429||[500,502,503,504].includes(status)?'unverified':'service_error';}
export function canonicalURL(input) {
  const url=new URL(input);
  if(!['https:','http:'].includes(url.protocol)||url.username||url.password)throw new TypeError('Unsupported URL');
  url.hash='';return url.href;
}
export function publicHostname(host) {
  host=host.toLowerCase().replace(/^\[|\]$/g,'').replace(/\.$/,'');
  if(['localhost','metadata','metadata.google.internal','instance-data','instance-data.ec2.internal'].includes(host)||/\.(localhost|local|internal)$/.test(host))return false;
  if(/^\d+(\.\d+){3}$/.test(host)) {
    const octets=host.split('.').map(Number);if(octets.some(x=>x>255))return false;
    const [a,b]=octets;return !(a===0||a===10||a===127||a>=224||a===169&&b===254||a===172&&b>=16&&b<=31||a===192&&b===168||a===100&&b>=64&&b<=127||a===198&&(b===18||b===19));
  }
  if(host.includes(':'))return !(/^::(?:0|1)?$/.test(host)||/^(fc|fd|fe[89ab]|ff)/.test(host)||host.startsWith('::ffff:'));
  return true;
}
