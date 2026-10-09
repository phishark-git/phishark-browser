// SPDX-License-Identifier: Apache-2.0
import fs from 'node:fs';
const vectors=JSON.parse(fs.readFileSync(new URL('../shared/test-vectors/decisions.json',import.meta.url)));
const literal=JSON.stringify;
const rows=vectors.map(({profile,response:r,expected})=>{
 const score=r.risk_calculation?.risk_score??r.risk_score;
 return `{phishark::Profile::${profile==='preflight'?'kPreflight':'kDeep'}, {${literal(r.scan_profile??'')}, ${typeof score==='number'?score:'std::nullopt'}, ${literal(r.verdict??'')}, ${literal(r.short_circuit_reason??'')}, ${literal(r.status??'')}, ${r.analysis_degraded===true}}, phishark::Verdict::${({safe:'kSafe',warning:'kWarning',blocked:'kBlocked',unverified:'kUnverified'})[expected]}}`;
});
const dir=new URL('../.build/native/',import.meta.url);fs.mkdirSync(dir,{recursive:true});
fs.writeFileSync(new URL('vectors.inc',dir),'#include <array>\nstruct Vector { phishark::Profile profile; phishark::Result result; phishark::Verdict expected; };\nconst std::array<Vector,'+rows.length+'> vectors = {{\n'+rows.join(',\n')+'\n}};\n');
