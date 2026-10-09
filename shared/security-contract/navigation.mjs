// SPDX-License-Identifier: Apache-2.0
import {canonicalURL,decide,profiles,trustedPreflight} from './policy.mjs';
export class NavigationSession {
  #generation=0;#controllers=new Set();#listeners=new Set();#trustedPreflight=false;
  constructor({privateMode=false,consent=false}={}){this.privateMode=privateMode;this.consent=consent;this.state=Object.freeze({policy:'unknown',url:'',generation:0});}
  begin(url){for(const c of this.#controllers)c.abort();this.#controllers.clear();this.#trustedPreflight=false;const generation=++this.#generation;this.#set({policy:'checking',url:canonicalURL(url),generation});return generation;}
  controller(){const c=new AbortController();this.#controllers.add(c);return c;}
  apply(generation,profile,decision){if(generation!==this.#generation||this.state.policy==='blocked')return false;if(profile===profiles.preflight)this.#trustedPreflight=trustedPreflight(decision);this.#set({...this.state,policy:decide(profile,decision),profile,decision});return true;}
  unavailable(generation,policy='unverified'){if(generation!==this.#generation||this.state.policy==='blocked')return false;this.#set({...this.state,policy});return true;}
  canCapture(){return !this.privateMode&&this.consent&&!this.#trustedPreflight&&this.state.policy!=='blocked';}
  subscribe(listener){this.#listeners.add(listener);return()=>this.#listeners.delete(listener);}
  close(){for(const c of this.#controllers)c.abort();this.#controllers.clear();this.#trustedPreflight=false;++this.#generation;this.#listeners.clear();this.state=Object.freeze({policy:'unknown',url:'',generation:this.#generation});}
  #set(state){this.state=Object.freeze(state);for(const listener of this.#listeners)listener(this.state);}
}
