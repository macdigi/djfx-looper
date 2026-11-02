#!/usr/bin/env python3
"""Build the stable native three-knob interface and on-device guide."""
from pathlib import Path
import argparse,base64,json,os,shutil,sys,hashlib,shlex,xml.etree.ElementTree as ET
from PIL import Image,ImageDraw
LEGACY_SCOPES=['PEAKS','WAVE','SPECTRUM','VECTOR','DEPTH','TRACE','BARS']
SCOPES=[]
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--tools',type=Path,required=True);p.add_argument('--unbranded',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;sys.path.insert(0,str(a.tools.resolve()));os.environ['MPC_VST_TOOLS']=str(a.tools.resolve());import shadow_skin
work=a.output/'skin-source';assets=work/'assets';assets.mkdir(parents=True,exist_ok=True)
shutil.copy2(here/'assets/ArchivoBlack.ttf',assets/'ArchivoBlack.ttf')
params=[]
def param(key,name,default=0,options=None):
 value={'key':key,'name':name,'min':0,'max':1,'default':default}
 if options:value.update(max=len(options)-1,options=options)
 params.append(value)
param('length','Length');param('speed','Speed',1);param('loop','Loop');param('enabled','FX ON',1)
param('scope','Legacy Scope',0,LEGACY_SCOPES);param('scope_on','Scope ON',1,['OFF','ON']);param('expand','Legacy Expand');param('identity','Display',64,[str(i) for i in range(65)]);param('palette','Ink',0,['MINT','VIOLET','CREAM'])
for key in ['surface','sliders','loopview','momentary','hold']:param(key,'Legacy '+key)
for i in range(64):param('plot%d'%i,'Legacy Display %02d'%(i+1))
for i in range(64):param('bank%d'%i,'Legacy Selector %02d'%(i+1))
(work/'params.json').write_text(json.dumps(params,indent=2)+'\n')
logo=''
if not a.unbranded:
 lock_path=here/'DEPENDENCIES.json' if (here/'DEPENDENCIES.json').exists() else here.parents[1]/'DEPENDENCIES.json'
 expected=json.loads(lock_path.read_text())['rareDataLogoSha256'];assert hashlib.sha256((here/'assets/rdlogo.png').read_bytes()).hexdigest()==expected,'Use the authentic Rare Data logo asset'
 data=base64.b64encode((here/'assets/rdlogo.png').read_bytes()).decode()
 logo='<defs><clipPath id="rd"><rect x="1034" y="92" width="80" height="80" rx="12"/></clipPath></defs><image x="1034" y="92" width="80" height="80" clip-path="url(#rd)" href="data:image/png;base64,'+data+'"/>'
background='''<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="800"><defs><linearGradient id="steel" x2="0" y2="1"><stop stop-color="#333e43"/><stop offset=".5" stop-color="#222b30"/><stop offset="1" stop-color="#121b20"/></linearGradient></defs><rect width="1280" height="800" fill="url(#steel)"/>'''+''.join('<path d="M0 %dH1280" stroke="#9eb1b8" stroke-opacity=".025"/>'%y for y in range(0,800,3))+'''<rect x="24" y="82" width="1232" height="106" rx="16" fill="#152027" stroke="#879899" stroke-opacity=".4"/><rect x="28" y="224" width="1224" height="344" rx="16" fill="#1d292f" stroke="#869396" stroke-opacity=".35"/><path d="M54 188H570" stroke="#b06cf5" stroke-width="3"/>'''+logo+'</svg>'
(assets/'face.svg').write_text(background)
(assets/'help-face.svg').write_text(background.replace('<rect x="28" y="224" width="1224" height="344"','<rect x="28" y="212" width="1224" height="510"').replace('fill="#1d292f" stroke="#869396" stroke-opacity=".35"','fill="#091316" stroke="#869396" stroke-opacity=".35"'))
(work/'public.css').write_text(':root{--font:"Archivo Black",sans-serif;--label-size:14px;--radius:10px;--sheen:.02}.knob-track,.knob-arc{stroke-width:6px}.knob-pointer{stroke-width:4px}')
lines=['art_css=public.css','theme_bg=222b30','theme_panel=1d292f','theme_line=657b80','theme_ink=e8f8f2','theme_ink_dim=9fbdb8','theme_accent=b06cf5','theme_accent_hi=d3a2ff','theme_knob_face=152027','theme_knob_ring=435459','theme_knob_dot=cf9aff','theme_box=1d292f','theme_btn_bg=273a40']
for page in ['PLAY','HELP']:
 lines+=['[tab '+page+']','art file=assets/'+('help-face.svg' if page=='HELP' else 'face.svg')+' x=0 y=0 w=1280 h=800','text cx=52 cy=128 label="DJFX LOOPER" size=3.1 align=left fontfile=assets/ArchivoBlack.ttf','text cx=52 cy=174 label="NATIVE EDITION" size=1.05 align=left color=9fbdb8 fontfile=assets/ArchivoBlack.ttf','knob cx=1180 cy=128 r=26 key=enabled label="FX ON"']
 if page=='HELP':
  help_lines=[
   (70,238,2.1,'QUICK GUIDE','e8f8f2'),
   (70,282,1.8,'LENGTH','cf9aff'),(70,314,1.7,'230 to 12 ms. Shorter lengths make tighter repeats.','c6dad6'),
   (70,352,1.8,'SPEED','cf9aff'),(70,384,1.7,'-100 reverses. 0 stops the read head. +100 plays forward.','c6dad6'),
   (70,422,1.8,'LOOP + BYPASS','cf9aff'),(70,454,1.7,'Turn LOOP past halfway to catch a loop; turn back to release.','c6dad6'),
   (70,484,1.7,'Length still shapes the sound with LOOP off.','c6dad6'),
   (70,516,1.7,'FX ON: OFF is dry; ON applies the effect.','c6dad6'),
   (70,558,2.0,'MORE FROM RARE DATA','e8f8f2'),
   (70,586,1.7,'Get DJFX Looper with enhanced scopes through Rare Data HQ.','c6dad6'),
   (70,618,1.7,'Also: QuotaFX / 64 effects, Quota Stems / separation, RD303 / bass.','c6dad6'),
   (70,666,1.9,'raredata.net  /  getquota.app/djfx-looper','cf9aff'),
  ]
  for x,y,size,label,color in help_lines:lines.append('text cx=%d cy=%d label="%s" size=%s align=left color=%s fontfile=assets/ArchivoBlack.ttf'%(x,y,label,size,color))
  lines.append('qlinks "HELP" = length,speed,loop,enabled')
  continue
 lines+=['knob cx=230 cy=374 r=72 key=length label="LENGTH"','knob cx=640 cy=374 r=72 key=speed label="SPEED"','knob cx=1050 cy=374 r=72 key=loop label="LOOP"','qlinks "DJFX LOOPER" = length,speed,loop,enabled',
 'text cx=70 cy=614 label="LOOP engages above halfway. FX ON switches dry / effected sound." size=1.6 align=left color=c6dad6 fontfile=assets/ArchivoBlack.ttf',
 'text cx=70 cy=662 label="HELP: quick guide + Rare Data HQ / enhanced scopes." size=1.6 align=left color=cf9aff fontfile=assets/ArchivoBlack.ttf']
layout=work/'layout.conf';layout.write_text('\n'.join(lines)+'\n')
renderer=work/'html_art_native_exec';renderer.write_text('#!/bin/sh\nexec '+shlex.quote(sys.executable)+' '+shlex.quote(str(here/'html_art_native.py'))+' \"$@\"\n');renderer.chmod(0o755)
folder=Path(shadow_skin.write_skin(str(a.output.resolve()),'Rare Data','DJFX Looper Native',str(layout.resolve()),params,str(renderer.resolve())))
version=folder/'version.xml';tree=ET.parse(version);tree.find('version').text='1.1.3.0';tree.write(version,encoding='utf-8',xml_declaration=True)
skin=folder/'Plugin Skins';file=skin/'TUI.json';doc=json.loads(file.read_text());definitions={v['key']:v['value'] for v in doc['pageData']['componentDefinitions']['localComponentDefinitions']}
for definition in definitions.values():
 for child in definition.get('componentsData',[]):
  data=child['componentData'].get('data',{});name=data.get('filmStrip')
  if name:
   with Image.open(skin/name) as original:
    im=original.convert('RGBA');side=im.width;frames=im.height//side
    if im.height>16384:
     count=min(96,16384//side);strip=Image.new('RGBA',(side,side*count))
     for frame in range(count):
      source=round(frame*(frames-1)/(count-1));strip.paste(im.crop((0,source*side,side,(source+1)*side)),(0,frame*side))
     strip.save(skin/name);frames=count
    data['numFrames']=frames
file.write_text(json.dumps(doc,indent=2)+'\n')
for name in ['ArchivoBlack-OFL.txt','BRANDING-LICENSE.txt']:shutil.copy2(here/'assets'/name,folder/name)
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
 with Image.open(skin/name) as im:decoded+=im.width*im.height*4;assert im.height<=16384
assert decoded<32*1024**2,decoded
for tab in doc['pageData']['tabs']:
 for c in definitions[tab['componentName']]['componentsData']:
  x,y,w,h=map(int,c['bounds']['bounds'].split());assert x>=0 and y>=0 and x+w<=1280 and y+h<=628
report={'nativeScopes':SCOPES,'parameters':142,'savedParameters':14,'decodedImageBytes':decoded,'filmstripScopeWidgets':0,'connectedInProcessRenderer':False,'displayBackend':None,'helpPage':True,'enhancedEditionURL':'https://getquota.app/djfx-looper','privateScopeWorker':False,'startupHook':False,'authenticRareDataLogo':not a.unbranded,'hardwareTested':False}
(folder/'skin-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(folder);print(json.dumps(report))
