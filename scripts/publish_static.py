"""Publish a static EXE and the source/object materials needed for Qt relinking."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--version', default='0.1.0')
args = parser.parse_args()
if not re.fullmatch(r'\d+\.\d+\.\d+([.-][A-Za-z0-9.-]+)?', args.version):
    parser.error('Invalid version')
root = Path(__file__).resolve().parent.parent
build = root / 'cmake-build-static'
kit = root / '.docs-tmp/qt-static/install'
source_zip = root / '.docs-tmp/qt-static/qtbase-6.11.2.zip'
expected = '8f8c16703a8170b235361aacdf0ec97d2445ae4e3e3d127eb1576f498269ef79'
if hashlib.file_digest(source_zip.open('rb'), 'sha256').hexdigest() != expected:
    raise RuntimeError('Qt source checksum mismatch')
output = root / 'packages'
output.mkdir(exist_ok=True)
base = f'ZeroTier-GUI-{args.version}-Windows-x64'
exe = output / f'{base}-Standalone.exe'
archive = output / f'{base}-StaticSupport.zip'
support = output / f'{base}-StaticSupport'
if any(p.exists() for p in (exe, archive, support)):
    raise RuntimeError('Static outputs already exist; preserve them and use a new version')

ninja = (build / 'build.ninja').read_text(encoding='utf-8')
block = re.search(r'^build zerotier_gui\.exe: (.*?)(?=\n\n)', ninja, re.M | re.S).group(1)
objects = block.split('\n', 1)[0].split(' | ', 1)[0].split(' ', 1)[1]
libraries = re.search(r'^  LINK_LIBRARIES = (.*)$', block, re.M).group(1)
flags = re.search(r'^  LINK_FLAGS = (.*)$', block, re.M).group(1)
response = objects + '\n' + libraries + '\n' + flags
response = response.replace('$:', ':').replace('$ ', ' ').replace('$$', '$')
for prefix in (str(kit), kit.as_posix(), str(kit).replace('\\', '/')):
    response = response.replace(prefix, 'qt')
if str(root).lower() in response.lower() or root.as_posix().lower() in response.lower():
    raise RuntimeError('Link response still contains workspace paths')

support.mkdir()
for obj in (build / 'CMakeFiles/zerotier_gui.dir').rglob('*.obj'):
    destination = support / obj.relative_to(build)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(obj, destination)
ela = Path('ElaWidgetTools-main/ElaWidgetTools/ElaWidgetTools.lib')
(support / ela).parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(build / ela, support / ela)
for folder in ('lib', 'plugins'):
    for file in (kit / folder).rglob('*'):
        if file.is_file() and file.suffix in ('.lib', '.obj'):
            destination = support / 'qt' / file.relative_to(kit)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
(support / 'link.rsp').write_text(response, encoding='utf-8')
(support / 'relink.cmd').write_text(
    '@echo off\ncd /d "%~dp0"\n'
    'link.exe /nologo @link.rsp /out:zerotier_gui.exe /MANIFEST:EMBED\n'
    'exit /b %errorlevel%\n', encoding='ascii')
shutil.copy2(source_zip, support / 'qtbase-everywhere-src-6.11.2.zip')
shutil.copy2(root / '.docs-tmp/qt-static/build/config.summary', support / 'qt-config.summary')
shutil.copytree(root / '.docs-tmp/qt-static/qtbase-everywhere-src-6.11.2/LICENSES', support / 'licenses/Qt')
shutil.copy2(root / 'ElaWidgetTools-main/LICENSE', support / 'licenses/ElaWidgetTools-MIT.txt')
shutil.copy2(root / 'packaging/STATIC.md', support / 'STATIC.md')
shutil.copytree(kit / 'sbom', support / 'sbom')
# Include the current source snapshot, including the new static build scripts.
tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
extra = ['scripts/BuildStatic.ps1', 'scripts/publish_static.py', 'packaging/STATIC.md']
for name in sorted(set(tracked + extra) - {''}):
    file = root / name
    if file.is_file():
        destination = support / 'source' / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, destination)
(support / 'README.txt').write_text(
    'ZeroTier GUI static support / Qt LGPL v3 relinking materials\n\n'
    'Run relink.cmd from an x64 MSVC developer command prompt to link the supplied\n'
    'application objects with the supplied Qt libraries. The rebuilt EXE requests\n'
    'administrator privileges, just like the original. No installer is required.\n'
    'To modify Qt, extract the included QtBase source, build it with the same options\n'
    'as source/scripts/BuildStatic.ps1, and replace the qt/lib and qt/plugins libraries\n'
    'and resource objects with the compatible rebuilt versions before relinking.\n'
    'The libraries use Release /MT and Qt 6.11.2 private headers; retain ABI compatibility.\n'
    'Alternatively rebuild the included application source with a modified static kit.\n'
    'These application objects are supplied for recombining/relinking with modified Qt;\n'
    'no restriction is imposed on this use or reverse engineering to debug Qt modifications.\n'
    'Qt LGPL/GPL and third-party texts are included in licenses and the source archive.\n'
    'ElaWidgetTools is MIT licensed. See STATIC.md and sbom for further build/license details.\n'
    'Publish these materials and notices alongside the standalone EXE. They are not\n'
    'required in the executable directory at runtime.\n', encoding='utf-8')
shutil.copy2(build / 'zerotier_gui.exe', exe)
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
    for file in support.rglob('*'):
        if file.is_file():
            bundle.write(file, file.relative_to(support))
checksums = [{'File': p.name, 'SHA256': hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()}
             for p in (exe, archive)]
(output / f'{base}-Standalone-SHA256.json').write_text(json.dumps(checksums, indent=2), encoding='utf-8')
print(f'Standalone EXE: {exe}')
print(f'Relink/source archive: {archive}')
