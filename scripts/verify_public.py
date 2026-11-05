"""Privacy and source-integrity checks. Never reads production-machine data."""
import argparse
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--package',type=Path)
args=parser.parse_args()
banned_suffixes={'.pem','.key','.pfx','.p12','.conf','.log','.env','.state','.pdb'}
markers=[b'-----BEGIN '+b'PRIVATE KEY-----',b'-----BEGIN '+b'RSA PRIVATE KEY-----',b'-----BEGIN '+b'CERTIFICATE-----']
token=re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}')

def audit_file(path):
    if path.suffix.lower() in banned_suffixes or path.name.startswith('.env'):
        raise SystemExit('Forbidden private-file type: '+path.name)
    if path.suffix.lower() in {'.md','.txt','.json','.py','.yml','.yaml','.cpp','.h','.cmake','.in','.ts'} or path.name in {'LICENSE','CMakeLists.txt'}:
        data=path.read_bytes()
        if any(marker in data for marker in markers) or token.search(data):
            raise SystemExit('Credential/certificate marker rejected: '+path.name)

if args.package:
    files=list(args.package.rglob('*'))
    for path in files:
        if path.is_file(): audit_file(path)
    for exe in ('deskflow.exe','deskflow-core.exe','deskflow-daemon.exe'):
        if not (args.package/exe).is_file(): raise SystemExit('Missing executable: '+exe)
    if any(path.name.lower() in {'settings','tls','.git'} for path in files):
        raise SystemExit('Settings, certificate, or Git directory must not be packaged.')
    print('Package audit passed: generated executables, dependencies, licenses and provenance only.')
else:
    manifest=json.loads((root/'SOURCE-MANIFEST.json').read_text())
    source=root/'upstream'
    actual={path.relative_to(source).as_posix():hashlib.sha256(path.read_bytes()).hexdigest() for path in source.rglob('*') if path.is_file()}
    if actual!=manifest: raise SystemExit('Source allowlist/hash mismatch.')
    allowed={'README.md','PROVENANCE.json','SOURCE-MANIFEST.json','.gitignore','.github/workflows/diagnostic-windows.yml','scripts/verify_public.py'}
    for path in root.rglob('*'):
        relative=path.relative_to(root).as_posix()
        if not path.is_file() or '.git' in path.relative_to(root).parts: continue
        if not relative.startswith('upstream/') and relative not in allowed:
            raise SystemExit('Unexpected repository file: '+relative)
        audit_file(path)
    workflow=(root/'.github/workflows/diagnostic-windows.yml').read_text()
    if 'runs-on: windows-2022' not in workflow or "github.repository_visibility == 'public'" not in workflow:
        raise SystemExit('Public standard-runner guard missing.')
    for forbidden in ('upload-artifact@','actions/cache@','self-hosted','schedule:','pull_request:','push:','windows-latest'):
        if forbidden in workflow: raise SystemExit('Unexpected workflow behavior: '+forbidden)
    if 'timeout-minutes: 35' not in workflow or 'workflow_dispatch:' not in workflow:
        raise SystemExit('Bounded manual workflow required.')
    print('Public source audit passed:',len(actual),'allowlisted source files; single manual standard Windows workflow.')
