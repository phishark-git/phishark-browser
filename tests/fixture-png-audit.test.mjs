// SPDX-License-Identifier: Apache-2.0
import test from 'node:test';
import assert from 'node:assert/strict';
import {deflateSync} from 'node:zlib';
import {auditPNG} from '../scripts/fixture-png-audit.mjs';

function png(){
  const chunk=(name,body)=>{const out=Buffer.alloc(body.length+12);out.writeUInt32BE(body.length);out.write(name,4);body.copy(out,8);return out;};
  const header=Buffer.alloc(13);header.writeUInt32BE(3);header.writeUInt32BE(1,4);header[8]=8;header[9]=2;
  return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',header),
    chunk('IDAT',deflateSync(Buffer.from([0,0,0,0,0,0,255,255,0,255]))),chunk('IEND',Buffer.alloc(0))]).toString('base64');
}
test('fixture pixel audit verifies opaque masks and independently detects color canaries',()=>{
  assert.deepEqual(auditPNG(png(),[[0,0,1,1]]),{width:3,height:1,pngBytes:Buffer.from(png(),'base64').length,
    maskCount:1,maskedPixels:1,magentaPixels:1,visibleBluePixels:1,nonBlackPixels:2});
  assert.throws(()=>auditPNG(png(),[[1,0,2,1]]),/unmasked pixel/);
  assert.throws(()=>auditPNG(png(),[[0,0,4,1]]),/mask bounds/);
  assert.throws(()=>auditPNG('cG5n'),/signature/);
});
