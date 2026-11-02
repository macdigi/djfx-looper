#!/usr/bin/env python3
"""Check native display bindings and exercise the installer in a disposable fixture."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile,xml.etree.ElementTree as ET,zipfile
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('skin',type=Path);p.add_argument('snapshot',type=Path);p.add_argument('package',type=Path);a=p.parse_args()
checks=0
def check(value,message):
    global checks
    checks+=1
    if not value:raise AssertionError(message)
data=json.loads((a.skin/'TUI.json').read_text())['pageData'];defs={x['key']:x['value'] for x in data['componentDefinitions']['localComponentDefinitions']}
snapshot=json.loads(a.snapshot.read_text());check(snapshot['syntheticAudio'],'Require actual synthetic analysis, not drawn example traces')
check(ET.parse(a.skin.parent/'version.xml').findtext('version')=='1.1.0.0','Native content version must be valid XML and match the package')
textures={}
for definition in defs.values():
    for child in definition.get('componentsData',[]):
        v=child['componentData'].get('data',{});name=v.get('filmStrip')
        if name:
            with Image.open(a.skin/name) as im:w,h=im.size
            check(h==w*v['numFrames'],'Filmstrip count mismatch')
            check(h<=16384,'Oversized device texture')
            textures[name]=w*h*4
check(sum(textures.values())<64*1024**2,'Too much decoded display artwork')
for tab in data['tabs']:
    components=defs[tab['componentName']]['componentsData']
    for c in components:
        x,y,w,h=map(int,c['bounds']['bounds'].split());check(x>=0 and y>=0 and x+w<=1280 and y+h<=628,'Control outside native screen')
    for mode in range(7):
        values=[0,1,0,1,mode/6,1,0,1,0,0,0,0,0,0]+snapshot['modes'][mode]
        shown=[]
        for c in components:
            if not c['componentData']['type'].startswith('shMeter_'):continue
            visible=True
            for handle in c['bounds']['additionalInvalidatingHandles']:
                m=re.fullmatch(r'IndexedEnabling/(\d+)/(\d+)/Parameter (\d+)',handle)
                if m:
                    selected,total,index=map(int,m.groups());check(0<=index<142,'Invalid display selector')
                    visible &= round(values[index]*(total-1))==selected
            if not visible:continue
            param=int(next(v['value'].split()[-1] for v in c['handle remapping']['map'] if v['key']=='Data'))
            check(14<=param<78,'Scope writes into musical controls')
            definition=defs[c['componentData']['type']];check(not definition['actions'],'Meter must be display-only')
            for child in definition['componentsData']:
                v=child['componentData'].get('data',{})
                if 'filmStrip' in v:check(0<=round(values[param]*(v['numFrames']-1))<v['numFrames'],'Frame index outside texture')
            shown.append(param)
        check(len(shown)==64 and len(set(shown))==64,'Each mode needs exactly one visible texture per data point')
qlinks=json.loads((a.skin/'Q-Links.json').read_text())['Screen Mode Q-Links']['map'][0]['Q-Links']
check([qlinks['Q-Link '+str(i)] for i in [13,9,5,1]]==[0,1,2,3],'Performance Q-Links changed')

with tempfile.TemporaryDirectory(prefix='djfx-native-install-fixture-') as temp:
    root=Path(temp);payload=root/'unpacked';payload.mkdir();settings=root/'MPC.settings';synths=root/'Synths';synths.mkdir();services=root/'services.log'
    with zipfile.ZipFile(a.package) as z:
        for info in z.infolist():
            check(not info.filename.startswith('/') and '..' not in Path(info.filename).parts,'Unsafe ZIP path')
        z.extractall(payload)
    folder=next(payload.iterdir())
    retained=synths/'Existing DJFX';retained.mkdir();(retained/'preset.bin').write_bytes(b'owner-preset-unchanged')
    projects=root/'Projects';projects.mkdir();(projects/'saved-project.xpj').write_bytes(b'owner-project-unchanged')
    original={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [retained/'preset.bin',projects/'saved-project.xpj']}
    settings.write_text('<PROPERTIES>\n  <VALUE name="pluginList-arm">\n    <KNOWNPLUGINS>\n      <PLUGIN name="Existing DJFX" file="/existing/DJFXLooper.so" uid="446a4c31"/>\n      <PLUGIN name="RD303" file="/existing/RD303.so" uid="52643333"/>\n    </KNOWNPLUGINS>\n  </VALUE>\n  <VALUE name="SynthContentLocations" val="'+str(synths)+'"/>\n</PROPERTIES>\n')
    environment={**os.environ,'MPC_INSTALL_TEST':'1','MPC_SETTINGS':str(settings),'MPC_TEST_LOG':str(services),'MPC_LEGACY_ROOT':str(root/'legacy')}
    def execute(script):
        result=subprocess.run(['sh',str(folder/script),'-y','-t',str(synths)],env=environment,capture_output=True,text=True)
        check(result.returncode==0,'Fixture '+script+': '+result.stdout+result.stderr)
    def entries():return {v.attrib['uid']:v.attrib for v in ET.parse(settings).getroot().iter('PLUGIN')}
    before=entries()
    for _ in range(2):
        execute('install.sh');e=entries();check(set(e)=={'446a4c31','52643333','446a4e73'},'Installer removed another plugin or duplicated the native identity');check(all(e[k]==v for k,v in before.items()),'Another plugin registration changed')
    execute('uninstall.sh');check(set(entries())=={'446a4c31','52643333'},'Uninstall removed another product')
    for name,digest in original.items():check(hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,'Musical data changed')
    check(services.read_text().splitlines()==['stop','start']*3,'Fixture bypassed the existing instrument service')
    baseline=settings.read_bytes();service_baseline=services.read_bytes()
    (folder/'portable/Rare Data - VST - DJFX Looper Native/DJFXLooperNative.so').write_bytes(b'corrupt-package')
    result=subprocess.run(['sh',str(folder/'install.sh'),'-y','-t',str(synths)],env=environment,capture_output=True)
    check(result.returncode!=0,'Corrupt package was accepted')
    check(settings.read_bytes()==baseline and services.read_bytes()==service_baseline,'Corrupt package changed settings or restarted the service')
print(json.dumps({'checks':checks,'failed':0,'skipped':0,'installerFixtureOnly':True,'hardwareTested':False}))
