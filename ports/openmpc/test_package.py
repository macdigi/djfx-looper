#!/usr/bin/env python3
"""Check native display bindings and exercise the installer in a disposable fixture."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile,xml.etree.ElementTree as ET,zipfile
from PIL import Image

p=argparse.ArgumentParser();p.add_argument('skin',type=Path);p.add_argument('package',type=Path);a=p.parse_args()
checks=0
def check(value,message):
    global checks
    checks+=1
    if not value:raise AssertionError(message)
data=json.loads((a.skin/'TUI.json').read_text())['pageData'];defs={x['key']:x['value'] for x in data['componentDefinitions']['localComponentDefinitions']}
check(json.loads((a.skin.parent/'skin-verification.json').read_text())['nativeScopes']==[],'Native must ship without experimental scopes')
check(ET.parse(a.skin.parent/'version.xml').findtext('version')=='1.1.3.0','Content version must be valid XML and match the package')
textures={}
for definition in defs.values():
    for child in definition.get('componentsData',[]):
        v=child['componentData'].get('data',{});name=v.get('filmStrip')
        if name:
            with Image.open(a.skin/name) as im:w,h=im.size
            check(h==w*v['numFrames'],'Filmstrip count mismatch');check(h<=16384,'Oversized device texture');textures[name]=w*h*4
check(sum(textures.values())<32*1024**2,'Too much decoded native artwork')
check([tab['tabName'] for tab in data['tabs']]==['DJFX LOOPER','HELP'],'Missing native guide/display navigation')
for tab in data['tabs']:
    components=defs[tab['componentName']]['componentsData']
    check(not any(c['componentData']['type'].startswith('shMeter_') for c in components),'Live scopes must not depend on non-refreshing native parameter meters')
    for c in components:
        x,y,w,h=map(int,c['bounds']['bounds'].split());check(x>=0 and y>=0 and x+w<=1280 and y+h<=628,'Control outside native screen')
    check(not any(c['bounds'].get('additionalInvalidatingHandles') for c in components),'Retired display identity markers must not remain')
help_components=defs[next(t['componentName'] for t in data['tabs'] if t['tabName']=='HELP')]['componentsData']
check(not any(c['bounds'].get('additionalInvalidatingHandles') for c in help_components),'Help must not carry live drawing markers')
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
