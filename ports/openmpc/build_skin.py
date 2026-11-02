#!/usr/bin/env python3
"""Rebuild the source edition's original native skin; no private UI or worker."""
from pathlib import Path
import argparse, json, math, os, shutil, sys
from PIL import Image, ImageDraw

SCOPES=['PEAKS','WAVE','SPECTRUM','VECTOR','DEPTH','TRACE','BARS']
parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path)
parser.add_argument('--tools',type=Path,required=True)
args=parser.parse_args()
here=Path(__file__).resolve().parent
sys.path.insert(0,str(args.tools.resolve()))
os.environ['MPC_VST_TOOLS']=str(args.tools.resolve())
import shadow_skin
work=args.output/'skin-source';assets=work/'assets';assets.mkdir(parents=True,exist_ok=True)
shutil.copy2(here/'assets/ArchivoBlack.ttf',assets/'ArchivoBlack.ttf')

def write_strip(name,mode,half=0):
    point=mode in (3,5);side=124 if point else 64 if mode==4 else 128
    frames=129 if point else 97
    strip=Image.new('RGBA',(side,side*frames))
    for index in range(1,frames):
        frame=Image.new('RGBA',(side,side));draw=ImageDraw.Draw(frame)
        if point:
            cell=index-1+128*half;x=7+(cell%16)*(side-14)/15;y=side-7-(cell//16)*(side-14)/15
            draw.ellipse((x-2.4,y-2.4,x+2.4,y+2.4),fill=(138,239,220,210))
            draw.ellipse((x-1,y-1,x+1,y+1),fill=(222,255,248,255))
        else:
            bank,level=divmod(index-1,32);amount=level/31
            color=[(126,216,202),(176,108,245),(217,246,236)][bank]
            x=side/2;top=8;bottom=side-8
            if mode==1:
                y=bottom-(bottom-top)*amount
                draw.line((x,side/2,x,y),fill=(*color,35),width=1)
                draw.ellipse((x-2.2,y-2.2,x+2.2,y+2.2),fill=(*color,255))
            else:
                height=(bottom-top)*amount*(.5 if mode==0 else 1)
                y0=side/2-height if mode==0 else bottom-height
                y1=side/2+height if mode==0 else bottom
                if height>0:draw.rounded_rectangle((x-4,y0,x+4,y1),radius=2,fill=(*color,220))
        strip.paste(frame,(0,index*side))
    path=assets/name;strip.save(path)
    return path,side,frames

strips={mode:write_strip('scope-%d.png'%mode,mode) for mode in range(7)}
# Identical geometry shares a decoded texture across scope modes.
strips[5]=strips[3];strips[6]=strips[2]
point_other=write_strip('scope-point-bottom.png',3,1)
params=[]
def param(key,name,default=0,options=None):
    value={'key':key,'name':name,'min':0,'max':1,'default':default}
    if options:value.update(max=len(options)-1,options=options)
    params.append(value)
param('length','Length');param('speed','Speed',1);param('loop','Loop');param('enabled','FX ON',1)
param('scope','Scope',0,SCOPES);param('scope_on','Scope ON',1,['OFF','ON'])
param('expand','Legacy Expand');param('identity','Legacy Display');param('palette','Ink',0,['MINT','VIOLET','CREAM'])
for key in ['surface','sliders','loopview','momentary','hold']:param(key,'Legacy '+key)
for i in range(64):param('plot%d'%i,'Display %02d'%(i+1))
for i in range(64):param('bank%d'%i,'Display Bank %02d'%(i+1))
(work/'params.json').write_text(json.dumps(params,indent=2)+'\n')

# The background is a new vector drawing, not a private bitmap faceplate.
background='''<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="800">
<defs><linearGradient id="steel" x2="0" y2="1"><stop stop-color="#333e43"/><stop offset=".5" stop-color="#222b30"/><stop offset="1" stop-color="#121b20"/></linearGradient></defs>
<rect width="1280" height="800" fill="url(#steel)"/>
'''+''.join('<path d="M0 %dH1280" stroke="#9eb1b8" stroke-opacity=".025"/>'%y for y in range(0,800,3))+'''
<rect x="24" y="82" width="1232" height="106" rx="16" fill="#152027" stroke="#879899" stroke-opacity=".4"/>
<rect x="28" y="212" width="1224" height="266" rx="12" fill="#091316" stroke="#76cebc" stroke-opacity=".38"/>
<path d="M80 345H1200" stroke="#7bbdab" stroke-opacity=".12"/>
<rect x="28" y="498" width="1224" height="224" rx="16" fill="#1d292f" stroke="#869396" stroke-opacity=".35"/>
<rect x="1034" y="94" width="82" height="78" rx="12" fill="#ff004d"/><text x="1075" y="124" text-anchor="middle" fill="#16040c" font-family="Archivo Black" font-size="20">RARE</text><text x="1075" y="151" text-anchor="middle" fill="#16040c" font-family="Archivo Black" font-size="20">DATA</text>
<path d="M54 188H570" stroke="#b06cf5" stroke-width="3"/>
</svg>'''
(assets/'face.svg').write_text(background)
lines=['art_css=public.css','theme_bg=222b30','theme_panel=1d292f','theme_line=657b80','theme_ink=e8f8f2','theme_ink_dim=9fbdb8','theme_accent=b06cf5','theme_accent_hi=d3a2ff','theme_knob_face=152027','theme_knob_ring=435459','theme_knob_dot=cf9aff','theme_box=1d292f','theme_btn_bg=273a40']
(work/'public.css').write_text(':root{--font:"Archivo Black",sans-serif;--label-size:14px;--title-size:20px;--radius:10px;--sheen:.02}.knob-track,.knob-arc{stroke-width:6px}.knob-pointer{stroke-width:4px}')
for page in ['PLAY','SETUP']:
    lines+=['[tab '+page+']','art file=assets/face.svg x=0 y=0 w=1280 h=800',
            'text cx=52 cy=128 label="DJFX LOOPER" size=3.1 align=left fontfile=assets/ArchivoBlack.ttf',
            'text cx=52 cy=174 label="RARE DATA / NATIVE SCOPE EDITION" size=1.05 align=left color=9fbdb8 fontfile=assets/ArchivoBlack.ttf',
            'knob cx=1180 cy=128 r=26 key=enabled label="FX ON"']
    for mode in range(7):
        _,side,frames=strips[mode]
        for i in range(64):
            if mode in (3,5):cx,cy=640,345
            elif mode==4:cx,cy=100+(i%16)*72,250+(i//16)*60
            else:cx,cy=136+i*16,345
            source=strips[mode][0].name
            lines.append('meter cx=%d cy=%d w=%d h=%d key=plot%d strip=assets/%s frames=%d when=scope:%s'%(cx,cy,side,side,i,source,frames,SCOPES[mode]))
            if mode in (3,5):lines.append('meter cx=%d cy=%d w=%d h=%d key=plot%d strip=assets/scope-point-bottom.png frames=%d when=scope:%s'%(cx,cy,side,side,i,frames,SCOPES[mode]))
    if page=='PLAY':
        lines+=['knob cx=230 cy=580 r=58 key=length label="LENGTH"','knob cx=640 cy=580 r=58 key=speed label="SPEED"','knob cx=1050 cy=580 r=58 key=loop label="LOOP"','qlinks "DJFX LOOPER" = length,speed,loop,enabled,scope,palette,scope_on']
    else:
        lines+=['knob cx=230 cy=580 r=54 key=scope label="SCOPE"','knob cx=640 cy=580 r=54 key=palette label="BAR/WAVE INK"','knob cx=1050 cy=580 r=54 key=scope_on label="SCOPE ON"','qlinks "DISPLAY" = scope,palette,scope_on,enabled,length,speed,loop']
layout=work/'layout.conf';layout.write_text('\n'.join(lines)+'\n')
folder=Path(shadow_skin.write_skin(str(args.output.resolve()),'Rare Data','DJFX Looper Native',str(layout.resolve()),params,str(here/'html_art_native.py')))
skin=folder/'Plugin Skins';file=skin/'TUI.json';doc=json.loads(file.read_text());page=doc['pageData']
definitions={d['key']:d['value'] for d in page['componentDefinitions']['localComponentDefinitions']}
replaced=set()
for tab in page['tabs']:
    point_seen={}
    for component in definitions[tab['componentName']]['componentsData']:
        kind=component['componentData']['type']
        if not kind.startswith('shMeter_'):continue
        condition=next(v for v in component['bounds']['additionalInvalidatingHandles'] if v.startswith('IndexedEnabling/'))
        mode=int(condition.split('/')[1]);source,side,frames=strips[mode]
        if mode in (3,5):
            # The paired half-plane textures stay below the device's 16K image
            # height without reducing XY resolution. Each point gets its own
            # display-only selector, independent of automation and preset state.
            index=int(next(v['value'].split()[-1] for v in component['handle remapping']['map'] if v['key']=='Data'))-14
            # Component order is top, bottom for each point on each scope page.
            seen=point_seen.get((mode,index),0);point_seen[(mode,index)]=seen+1;bank=seen%2
            component['bounds']['additionalInvalidatingHandles'].append('IndexedEnabling/%d/2/Parameter %d'%(bank,78+index))
            if bank:source,side,frames=point_other
        for child in definitions[kind]['componentsData']:
            data=child['componentData'].get('data',{})
            if 'filmStrip' in data:
                # Avoid the generator's 128-frame resampling; the analyzer emits
                # exact 97/129-frame indices, including transparent frame zero.
                data['filmStrip']=source.name;data['numFrames']=frames
                shutil.copy2(source,skin/source.name);replaced.add(kind)
for definition in definitions.values():
    for child in definition.get('componentsData',[]):
        data=child['componentData'].get('data',{});name=data.get('filmStrip')
        if name and not name.startswith('scope-'):
            with Image.open(skin/name) as im:data['numFrames']=im.height//im.width
file.write_text(json.dumps(doc,indent=2)+'\n')
shutil.copy2(here/'assets/ArchivoBlack-OFL.txt',folder/'ArchivoBlack-OFL.txt')
# Native font data is baked into owned artwork; do not bundle host fonts/files.
referenced=set()
def visit(v):
    if isinstance(v,str) and v.endswith('.png'):referenced.add(v)
    elif isinstance(v,list):
        for x in v:visit(x)
    elif isinstance(v,dict):
        for x in v.values():visit(x)
visit(doc)
for png in skin.glob('*.png'):
    if png.name not in referenced:png.unlink()
decoded=0
for name in referenced:
    with Image.open(skin/name) as im:decoded+=im.width*im.height*4
assert decoded<64*1024**2,decoded
for tab in page['tabs']:
    for c in definitions[tab['componentName']]['componentsData']:
        x,y,w,h=map(int,c['bounds']['bounds'].split());assert x>=0 and y>=0 and x+w<=1280 and y+h<=628,(c['componentData']['name'],x,y,w,h)
report={'nativeScopes':SCOPES,'parameters':142,'savedParameters':14,'decodedImageBytes':decoded,'meterDefinitions':len(replaced),'privateScopeWorker':False,'hardwareTested':False}
(folder/'skin-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(folder);print(json.dumps(report))
