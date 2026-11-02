#!/usr/bin/env python3
"""Offline native-asset layout review; no synthetic scope or hardware claim.

These are layout reviews, not screenshots or evidence of a working MPC display.
"""
from pathlib import Path
import argparse,json,re
from PIL import Image,ImageDraw,ImageFont

p=argparse.ArgumentParser();p.add_argument('skin',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
doc=json.loads((a.skin/'TUI.json').read_text())['pageData']
defs={v['key']:v['value'] for v in doc['componentDefinitions']['localComponentDefinitions']}
a.output.mkdir(parents=True,exist_ok=True)
font=ImageFont.truetype(str(Path(__file__).parent/'assets/ArchivoBlack.ttf'),16)
def box(value):return tuple(map(int,value['bounds'].split()))
cache={}
def load(name):
    if name not in cache:
        with Image.open(a.skin/name) as im:cache[name]=im.convert('RGBA')
    return cache[name]
for page,tab in enumerate(doc['tabs']):
    for mode in [1]:
        im=Image.new('RGBA',(1280,628),(10,18,22,255));draw=ImageDraw.Draw(im)
        values=[.5,.8,1,1,mode/6,1,0,1/64,0,0,0,0,0,0]+[0]*128
        for c in defs[tab['componentName']]['componentsData']:
            conditions=c['bounds'].get('additionalInvalidatingHandles',[])
            if any((m:=re.fullmatch(r'IndexedEnabling/(\d+)/(\d+)/Parameter (\d+)',v)) and round(values[int(m[3])]*(int(m[2])-1))!=int(m[1]) for v in conditions):continue
            cd=c['componentData'];x,y,w,h=box(c['bounds'])
            binding=c.get('handle remapping',{}).get('map',[])
            param=next((int(v['value'].split()[-1]) for v in binding if v['key']=='Data' and v['value'].startswith('Parameter ')),None)
            if cd['type']=='Image':im.alpha_composite(load(cd['data']['image']),(x,y));continue
            for child in defs[cd['type']].get('componentsData',[]):
                sd=child['componentData'];sx,sy,sw,sh=box(child['bounds']);data=sd.get('data',{})
                if sd['type']=='Knob':
                    strip=load(data['filmStrip']);side=strip.width;frames=strip.height//side
                    frame=round((values[param] if param is not None else 0)*(frames-1))
                    piece=strip.crop((0,frame*side,side,(frame+1)*side));im.alpha_composite(piece,(x+sx,y+sy))
                elif sd['type']=='Label' and param is not None:
                    label=cd['name'] if data.get('type')=='Name' else ('0.053 s' if param==0 else '60' if param==1 else 'ON' if param in (2,3,5) else ['PEAKS','WAVE','SPECTRUM','VECTOR','DEPTH','TRACE','BARS'][mode] if param==4 else 'MINT' if param==8 else '')
                    if label:
                        bounds=draw.textbbox((0,0),label,font=font);draw.text((x+sx+(sw-bounds[2])/2,y+sy+3),label,font=font,fill='#d9eee8')
        name='help.png' if tab['tabName']=='HELP' else 'play.png';im.convert('RGB').save(a.output/name)
contact=Image.new('RGB',(1280,628*2),(15,20,24));
for row,name in enumerate(['play.png','help.png']):
    with Image.open(a.output/name) as im:contact.paste(im,(0,row*628))
contact.save(a.output/'review.png')
print('Rendered 2 native-asset layout reviews; no hardware claim.')
