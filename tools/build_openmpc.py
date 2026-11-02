#!/usr/bin/env python3
"""One-command source build; never connects to hardware or publishes a release."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,tarfile,zipfile,re

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=Path('build/release'));p.add_argument('--tools',type=Path);p.add_argument('--bench',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
lock=json.loads((root/'DEPENDENCIES.json').read_text())
def run(command,**kwargs):subprocess.run([str(x) for x in command],check=True,**kwargs)
if a.tools:tools=a.tools.resolve()
else:
    cache=out/'dependencies';cache.mkdir(exist_ok=True);archive=cache/'openmpc-tools.tar.gz'
    if not archive.exists():
        run(['curl','--fail','--silent','--show-error','--location','--proto','=https','--tlsv1.2','--max-time','90','--output',archive,lock['toolsArchive']])
    assert hashlib.sha256(archive.read_bytes()).hexdigest()==lock['toolsArchiveSha256'],'Build-tool download checksum mismatch'
    target=cache/('mpc-vst-plugins-'+lock['toolsRevision'])
    if not (target/'tools/shadow_skin.py').exists():
        with tarfile.open(archive) as tar:
            for item in tar.getmembers():
                parts=Path(item.name).parts
                if len(parts)<2 or (parts[1]!='tools' and parts[1]!='LICENSE'):continue
                assert not item.issym() and not item.islnk() and '..' not in parts and not item.name.startswith('/')
                tar.extract(item,cache)
    tools=target/'tools'
for name,expected in lock['toolsFiles'].items():assert hashlib.sha256((tools/name).read_bytes()).hexdigest()==expected,name
native=out/'native';arm=out/'armv7'
run(['cmake','-S',root,'-B',native,'-DCMAKE_BUILD_TYPE=Release'])
run(['cmake','--build',native,'--parallel','3']);run(['ctest','--test-dir',native,'--output-on-failure'])
assert subprocess.check_output(['zig','version'],text=True).strip()==lock['zigVersion'],'Use the pinned Zig version'
run(['cmake','-S',root,'-B',arm,'-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF','-DCMAKE_TOOLCHAIN_FILE='+str(root/'ports/mpc/toolchain.cmake')])
run(['cmake','--build',arm,'--parallel','3'])
run([sys.executable,root/'ports/openmpc/build_skin.py',out/'skin','--tools',tools])
skin=out/'skin/Rare Data - VST - DJFX Looper Native';entry=out/'pluginlist-entry.xml'
entry.write_text('<PLUGIN name="DJFX Looper Native" descriptiveName="DJFX Looper Native" format="VST" category="Fx" manufacturer="Rare Data" version="1.1.1" file="/sdcard/vst/DJFXLooperNative.so" uid="446a4e73" isInstrument="0" fileTime="0" infoUpdateTime="0" numInputs="2" numOutputs="2" isShell="0"/>\n')
zig_env=subprocess.check_output(['zig','env'],text=True)
lib_dir=Path(re.search(r'\.lib_dir\s*=\s*"([^"]+)"',zig_env).group(1))
zig_exe=Path(re.search(r'\.zig_exe\s*=\s*"([^"]+)"',zig_env).group(1))
licenses=[lib_dir/'libcxx/LICENSE.TXT',lib_dir/'libcxxabi/LICENSE.TXT',lib_dir/'libunwind/LICENSE.TXT',zig_exe.parent.parent/'LICENSE']
runtime_notice=out/'Compiler-Runtime-Notices.txt';seen=set();parts=[]
for notice in licenses:
    assert notice.is_file(),'Missing pinned compiler runtime license: '+str(notice)
    text=notice.read_text();digest=hashlib.sha256(text.encode()).hexdigest()
    if digest not in seen:parts.append(notice.parent.name+' / '+notice.name+'\n\n'+text);seen.add(digest)
runtime_notice.write_text('\n\n'.join(parts)+'\n')
release_command=[sys.executable,tools/'release.py','--so',arm/'ports/mpc/DJFXLooperNative.so','--skin',skin,'--entry',entry,'--version','1.1.1','--id','djfx-looper','--repo','macdigi/djfx-looper','--license','Apache-2.0','--about','Rare Data short-loop effect with three Q-Link knobs and native scopes. See release notes for validated device profiles.','--extra',str(root/'LICENSE')+':LICENSE','--extra',str(root/'NOTICE')+':NOTICE','--extra',str(runtime_notice)+':Compiler-Runtime-Notices.txt','--extra',str(root/'ports/openmpc/assets/OpenMPC-MIT.txt')+':OpenMPC-MIT.txt','-o',out/'dist']
if a.bench:
    measured=json.loads(a.bench.read_text());assert measured['moduleSha256']==hashlib.sha256((arm/'ports/mpc/DJFXLooperNative.so').read_bytes()).hexdigest(), 'CPU report is for another binary'
    assert measured['verdict'] in ['PASS','WARN']
    normalized_bench=out/'bench-for-package.json';normalized_bench.write_text(json.dumps(measured)+'\n');release_command+=['--bench',normalized_bench]
run(release_command)
packages=[out/'dist/DJFX-Looper-Native-1.1.1-mpc-armv7.zip'];assert packages[0].is_file()
# The pinned tool's generic INSTALL text describes its earlier Force test.
# Replace that statement for this untested edition, retaining standard scripts.
with zipfile.ZipFile(packages[0]) as archive:
    infos=archive.infolist();files={i.filename:archive.read(i) for i in infos}
install=next(n for n in files if n.endswith('/INSTALL.md'))
text=files[install].decode().replace('- Tested on MPC OS with a Force. Installing plugins this way is unofficial: back up first, use at your own risk.', '- See the source repository tested.json and release notes for specific device/firmware results. Installing plugins this way is unofficial: preserve your saved projects and use at your own risk.')
files[install]=text.encode()
top=install.rsplit('/',1)[0];sums=top+'/SHA256SUMS'
files[sums]=('\n'.join(hashlib.sha256(data).hexdigest()+'  '+name[len(top)+1:] for name,data in sorted(files.items()) if name!=sums)+'\n').encode()
with zipfile.ZipFile(packages[0],'w',zipfile.ZIP_DEFLATED) as archive:
    for info in infos:archive.writestr(info,files[info.filename])
run([sys.executable,tools/'catalog_check.py',packages[0],'--catalog','--expect-id','djfx-looper','--expect-repo','macdigi/djfx-looper'])
with (out/'scope-snapshot.json').open('w') as stream:run([native/'ports/mpc/DJFXNativeScopeSnapshot'],stdout=stream)
run([sys.executable,root/'ports/openmpc/test_package.py',skin/'Plugin Skins',out/'scope-snapshot.json',packages[0]])

record={'package':packages[0].name,'sha256':hashlib.sha256(packages[0].read_bytes()).hexdigest(),'hardwareTested':False,'published':False,'sourceOnlyDependencies':True,'privateScopeWorker':False,'version':'1.1.1','uid':'446a4e73'}
(out/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
print('Offline package ready. Hardware/CPU acceptance is still required; nothing was installed or published.')
