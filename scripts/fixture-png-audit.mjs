// SPDX-License-Identifier: Apache-2.0
// Bounded synthetic-fixture PNG reader; never used by production capture.
import {inflateSync} from 'node:zlib';

export function auditPNG(base64, regions=[]) {
  const png=Buffer.from(base64,'base64');
  if(png.length<33||png.length>2*1024*1024||!png.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10])))throw Error('fixture PNG signature/size');
  const width=png.readUInt32BE(16),height=png.readUInt32BE(20),depth=png[24],color=png[25];
  if(!width||!height||width>4096||height>4096||width*height>12*1024*1024||depth!==8||![2,6].includes(color)||png[28]!==0)throw Error('fixture PNG format');
  const chunks=[];
  for(let offset=8;offset+12<=png.length;){
    const length=png.readUInt32BE(offset),end=offset+12+length;
    if(end>png.length)throw Error('fixture PNG chunk');
    if(png.toString('ascii',offset+4,offset+8)==='IDAT')chunks.push(png.subarray(offset+8,offset+8+length));
    offset=end;
  }
  const channels=color===6?4:3,stride=width*channels;
  const raw=inflateSync(Buffer.concat(chunks),{maxOutputLength:height*(stride+1)});
  if(raw.length!==height*(stride+1))throw Error('fixture PNG raster');
  const pixels=Buffer.alloc(width*height*channels);
  const paeth=(a,b,c)=>{const p=a+b-c,pa=Math.abs(p-a),pb=Math.abs(p-b),pc=Math.abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;};
  for(let y=0;y<height;y++){
    const filter=raw[y*(stride+1)];if(filter>4)throw Error('fixture PNG filter');
    for(let x=0;x<stride;x++){
      const offset=y*stride+x,a=x>=channels?pixels[offset-channels]:0,b=y?pixels[offset-stride]:0,c=y&&x>=channels?pixels[offset-stride-channels]:0;
      pixels[offset]=(raw[y*(stride+1)+1+x]+[0,a,b,Math.floor((a+b)/2),paeth(a,b,c)][filter])&255;
    }
  }
  let magentaPixels=0,visibleBluePixels=0,nonBlackPixels=0,maskedPixels=0;
  for(let i=0;i<pixels.length;i+=channels){
    const [r,g,b]=pixels.subarray(i,i+3);
    if(r>230&&g<25&&b>230)magentaPixels++;
    if(r<25&&g<25&&b>230)visibleBluePixels++;
    if(r||g||b)nonBlackPixels++;
  }
  for(const rect of regions){
    if(!Array.isArray(rect)||rect.length!==4||!rect.every(Number.isInteger))throw Error('fixture mask geometry');
    const [l,t,r,b]=rect;if(l<0||t<0||r>width||b>height||l>r||t>b)throw Error('fixture mask bounds');
    for(let y=t;y<b;y++)for(let x=l;x<r;x++){
      const i=(y*width+x)*channels;
      if(pixels[i]||pixels[i+1]||pixels[i+2]||(channels===4&&pixels[i+3]!==255))throw Error('fixture unmasked pixel');
      maskedPixels++;
    }
  }
  return {width,height,pngBytes:png.length,maskCount:regions.length,maskedPixels,
    magentaPixels,visibleBluePixels,nonBlackPixels};
}
