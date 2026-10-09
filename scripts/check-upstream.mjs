// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
const pins=JSON.parse(fs.readFileSync(new URL('../shared/security-contract/upstreams.lock.json',import.meta.url)));
for(const platform of ['android','ios']){
 const pin=pins[platform],repo=new URL(pin.repository).pathname.slice(1).replace(/\.git$/,'');
 const response=await fetch(`https://api.github.com/repos/${repo}/releases/latest`,{headers:{Accept:'application/vnd.github+json'},signal:AbortSignal.timeout(15000)});
 if(!response.ok)throw new Error(`${platform}: upstream release check unavailable (${response.status})`);
 const release=await response.json();
 console.log(`${platform}: pinned ${pin.tag}; latest ${release.tag_name}; ${release.html_url}`);
 if(release.tag_name!==pin.tag)process.exitCode=2;
}
