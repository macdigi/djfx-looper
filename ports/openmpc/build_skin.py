#!/usr/bin/env python3
"""Build native Q-Link controls and a marked panel for the public live renderer."""
from pathlib import Path
import argparse,base64,json,os,shutil,sys,hashlib,xml.etree.ElementTree as ET
from PIL import Image,ImageDraw
SCOPES=['PEAKS','WAVE','SPECTRUM','VECTOR','DEPTH','TRACE','BARS']
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
param('scope','Scope',0,SCOPES);param('scope_on','Scope ON',1,['OFF','ON']);param('expand','Legacy Expand');param('identity','Display',64,[str(i) for i in range(65)]);param('palette','Ink',0,['MINT','VIOLET','CREAM'])
for key in ['surface','sliders','loopview','momentary','hold']:param(key,'Legacy '+key)
for i in range(64):param('plot%d'%i,'Legacy Display %02d'%(i+1))
for i in range(64):param('bank%d'%i,'Legacy Selector %02d'%(i+1))
(work/'params.json').write_text(json.dumps(params,indent=2)+'\n')
def marker(name,slot,corner):
 colors=[0x0b1518,0x101a19+corner if corner else ((8+(slot>>3))<<16)|((16+(slot&7))<<8)|22,0x0c171a+corner,0x0a1417]
 im=Image.new('RGB',(16,4));d=ImageDraw.Draw(im)
 for k,color in enumerate(colors):d.rectangle((4*k,0,4*k+3,3),fill=(color>>16,(color>>8)&255,color&255))
 im.save(assets/name)
for i in range(64):marker('instance-%d.png'%i,i,0)
Image.new('RGB',(16,4),(9,19,22)).save(assets/'instance-64.png')
for corner in [1,2,3]:marker('corner-%d.png'%corner,0,corner)
logo=''
if not a.unbranded:
 lock_path=here/'DEPENDENCIES.json' if (here/'DEPENDENCIES.json').exists() else here.parents[1]/'DEPENDENCIES.json'
 expected=json.loads(lock_path.read_text())['rareDataLogoSha256'];assert hashlib.sha256((here/'assets/rdlogo.png').read_bytes()).hexdigest()==expected,'Use the authentic Rare Data logo asset'
 data=base64.b64encode((here/'assets/rdlogo.png').read_bytes()).decode()
 logo='<defs><clipPath id="rd"><rect x="1034" y="92" width="80" height="80" rx="12"/></clipPath></defs><image x="1034" y="92" width="80" height="80" clip-path="url(#rd)" href="data:image/png;base64,'+data+'"/>'
background='''<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="800"><defs><linearGradient id="steel" x2="0" y2="1"><stop stop-color="#333e43"/><stop offset=".5" stop-color="#222b30"/><stop offset="1" stop-color="#121b20"/></linearGradient></defs><rect width="1280" height="800" fill="url(#steel)"/>'''+''.join('<path d="M0 %dH1280" stroke="#9eb1b8" stroke-opacity=".025"/>'%y for y in range(0,800,3))+'''<rect x="24" y="82" width="1232" height="106" rx="16" fill="#152027" stroke="#879899" stroke-opacity=".4"/><rect x="28" y="212" width="1224" height="266" rx="12" fill="#091316" stroke="#76cebc" stroke-opacity=".38"/><rect x="28" y="498" width="1224" height="224" rx="16" fill="#1d292f" stroke="#869396" stroke-opacity=".35"/><path d="M54 188H570" stroke="#b06cf5" stroke-width="3"/>'''+logo+'</svg>'
(assets/'face.svg').write_text(background)
(work/'public.css').write_text(':root{--font:"Archivo Black",sans-serif;--label-size:14px;--radius:10px;--sheen:.02}.knob-track,.knob-arc{stroke-width:6px}.knob-pointer{stroke-width:4px}')
lines=['art_css=public.css','theme_bg=222b30','theme_panel=1d292f','theme_line=657b80','theme_ink=e8f8f2','theme_ink_dim=9fbdb8','theme_accent=b06cf5','theme_accent_hi=d3a2ff','theme_knob_face=152027','theme_knob_ring=435459','theme_knob_dot=cf9aff','theme_box=1d292f','theme_btn_bg=273a40']
for page in ['PLAY','SETUP']:
 lines+=['[tab '+page+']','art file=assets/face.svg x=0 y=0 w=1280 h=800','text cx=52 cy=128 label="DJFX LOOPER" size=3.1 align=left fontfile=assets/ArchivoBlack.ttf','text cx=52 cy=174 label="NATIVE SCOPE EDITION" size=1.05 align=left color=9fbdb8 fontfile=assets/ArchivoBlack.ttf','knob cx=1180 cy=128 r=26 key=enabled label="FX ON"']
 lines.append('picture x=40 y=224 w=16 h=4 key=identity files="'+','.join('assets/instance-%d.png'%i for i in range(65))+'"')
 for corner,x,y in [(1,1216,224),(2,40,464),(3,1216,464)]:lines.append('art file=assets/corner-%d.png x=%d y=%d w=16 h=4'%(corner,x,y))
 if page=='PLAY':lines+=['knob cx=230 cy=580 r=58 key=length label="LENGTH"','knob cx=640 cy=580 r=58 key=speed label="SPEED"','knob cx=1050 cy=580 r=58 key=loop label="LOOP"','qlinks "DJFX LOOPER" = length,speed,loop,enabled,scope,palette,scope_on']
 else:lines+=['knob cx=230 cy=580 r=54 key=scope label="SCOPE"','knob cx=640 cy=580 r=54 key=palette label="INK"','knob cx=1050 cy=580 r=54 key=scope_on label="SCOPE ON"','qlinks "DISPLAY" = scope,palette,scope_on,enabled,length,speed,loop']
layout=work/'layout.conf';layout.write_text('\n'.join(lines)+'\n')
folder=Path(shadow_skin.write_skin(str(a.output.resolve()),'Rare Data','DJFX Looper Native',str(layout.resolve()),params,str(here/'html_art_native.py')))
version=folder/'version.xml';tree=ET.parse(version);tree.find('version').text='1.1.1.0';tree.write(version,encoding='utf-8',xml_declaration=True)
skin=folder/'Plugin Skins';file=skin/'TUI.json';doc=json.loads(file.read_text());definitions={v['key']:v['value'] for v in doc['pageData']['componentDefinitions']['localComponentDefinitions']}
for definition in definitions.values():
 for child in definition.get('componentsData',[]):
  data=child['componentData'].get('data',{});name=data.get('filmStrip')
  if name:
   with Image.open(skin/name) as im:data['numFrames']=im.height//im.width
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
report={'nativeScopes':SCOPES,'parameters':142,'savedParameters':14,'decodedImageBytes':decoded,'filmstripScopeWidgets':0,'connectedInProcessRenderer':True,'privateScopeWorker':False,'startupHook':False,'authenticRareDataLogo':not a.unbranded,'hardwareTested':False}
(folder/'skin-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(folder);print(json.dumps(report))
