// SPDX-License-Identifier: Apache-2.0
import {canonicalURL,decide,errorPolicy,profiles} from './policy.mjs';
export class BrowserClient {
  constructor({baseURL,apiKey,fetchImpl=fetch,clock=Date.now,wait=ms=>new Promise(r=>setTimeout(r,ms)),budgets={preflight:10000,deep:20000}}){
    const base=new URL(baseURL);
    if(base.protocol!=='https:'&&!(base.protocol==='http:'&&['127.0.0.1','localhost'].includes(base.hostname)))throw new TypeError('HTTPS API required');
    if(base.username||base.password||base.search||base.hash)throw new TypeError('Invalid API base');
    if(typeof apiKey!=='string'||!apiKey.trim()||/[\r\n]/.test(apiKey))throw new TypeError('API key required');
    this.base=base.href.replace(/\/$/,'');this.apiKey=apiKey;this.fetch=fetchImpl;this.clock=clock;this.wait=wait;this.budgets=budgets;
  }
  async scan(profile,target,{evidence,privateMode=false,consent=false,signal}={}){
    if(!Object.values(profiles).includes(profile))throw new TypeError('Unknown profile');
    if(profile===profiles.deep&&(privateMode||!consent))throw new TypeError('Content capture not permitted');
    if(profile===profiles.preflight&&evidence!==undefined)throw new TypeError('Preflight is URL-only');
    if(profile===profiles.deep&&!evidence?.response?.html&&!evidence?.response?.screenshot)throw new TypeError('Evidence required');
    if(profile===profiles.deep&&new TextEncoder().encode(JSON.stringify(evidence)).length>9437184)throw new TypeError('Evidence too large');
    target=canonicalURL(target);const deep=profile===profiles.deep,budget=deep?this.budgets.deep:this.budgets.preflight;
    const controller=new AbortController(),abort=()=>controller.abort();
    signal?.addEventListener('abort',abort,{once:true});if(signal?.aborted)controller.abort();
    const timeout=setTimeout(abort,budget),deadline=this.clock()+budget;
    try{
      for(let attempt=0;attempt<2;attempt++){
        const response=await this.fetch(this.base+'/api/v1/browser/'+(deep?'deep':'preflight'),{method:'POST',redirect:'error',cache:'no-store',credentials:'omit',headers:{'Content-Type':'application/json','X-API-Key':this.apiKey},body:JSON.stringify(deep?{target,web_evidence:evidence}:{target}),signal:controller.signal});
        if(response.status===429){
          const errorText=await response.text();
          if(new TextEncoder().encode(errorText).length>1024*1024)return {policy:'unverified'};
          let code;try{code=JSON.parse(errorText).code;}catch{}
          if(['QUOTA_EXCEEDED','LIMIT_EXCEEDED','INSUFFICIENT_CREDITS'].includes(code))return {policy:'service_error',status:429,code};
          if(attempt===1)return {policy:'unverified',status:429};
          const seconds=Number(response.headers.get('retry-after')||0),delay=Math.min(2000,Math.max(0,Number.isFinite(seconds)?seconds*1000:0));
          if(this.clock()+delay>=deadline)return {policy:'unverified'};
          await this.wait(delay);if(controller.signal.aborted)return {policy:'unverified'};continue;
        }
        if(!response.ok){
          const errorText=await response.text();let code;
          if(new TextEncoder().encode(errorText).length>1024*1024)return {policy:'unverified'};
          try{code=JSON.parse(errorText).code;}catch{}
          return {policy:['BROWSER_NOT_CONFIGURED','QUOTA_EXCEEDED','LIMIT_EXCEEDED','INSUFFICIENT_CREDITS'].includes(code)?'service_error':errorPolicy(response.status),status:response.status,code};
        }
        const text=await response.text();if(new TextEncoder().encode(text).length>1024*1024)return {policy:'unverified'};
        const payload=JSON.parse(text),data=payload.data??payload;
        if(controller.signal.aborted||this.clock()>=deadline)return {policy:'unverified'};
        return {policy:decide(profile,data),data,checkedAt:new Date(this.clock()).toISOString()};
      }
    }catch{return {policy:'unverified',cancelled:signal?.aborted===true};}
    finally{clearTimeout(timeout);signal?.removeEventListener('abort',abort);}
  }
}
