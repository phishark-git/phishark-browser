// SPDX-License-Identifier: Apache-2.0
export class VerdictCache {
  #entries=new Map();#inflight=new Map();#generation=0;
  constructor(ttl,maxEntries=1000,clock=Date.now){this.ttl=ttl;this.maxEntries=maxEntries;this.clock=clock;}
  get(key){const e=this.#entries.get(key);if(!e||e.expires<=this.clock()){this.#entries.delete(key);return undefined;}this.#entries.delete(key);this.#entries.set(key,e);return e.value;}
  async check(key,factory){
    const cached=this.get(key);if(cached!==undefined)return cached;
    if(this.#inflight.has(key))return this.#inflight.get(key);
    const generation=this.#generation;
    const operation=Promise.resolve().then(factory).then(value=>{
      if(this.#generation===generation&&!['unverified','service_error'].includes(value.policy)){
        this.#entries.set(key,{value,expires:this.clock()+this.ttl});
        while(this.#entries.size>this.maxEntries)this.#entries.delete(this.#entries.keys().next().value);
      }return value;
    }).finally(()=>{if(this.#inflight.get(key)===operation)this.#inflight.delete(key);});
    this.#inflight.set(key,operation);return operation;
  }
  clear(){++this.#generation;this.#entries.clear();this.#inflight.clear();}
}
