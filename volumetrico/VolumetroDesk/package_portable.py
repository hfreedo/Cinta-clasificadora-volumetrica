"""Ejecutar después de PyInstaller; verifica una extracción nueva sin conectar hardware."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

root=Path(__file__).resolve().parent.parent
folder=root.parent/'Entregas'/'VolumetroDesk'
shutil.copy2(root/'VolumetroDesk'/'GUIA.md',folder/'GUIA.md')
firmware=folder/'firmware'/'Volumetrico_UNO'
firmware.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Volumetrico_UNO'/'Volumetrico_UNO.ino',firmware/'Volumetrico_UNO.ino')
archive=root.parent/'Entregas'/'VolumetroDesk_Windows_x64_v1.0.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
    for p in folder.rglob('*'):
        if p.is_file():z.write(p,p.relative_to(folder.parent))
qa=root/'VolumetroDesk'/'qa'
qa.mkdir(parents=True,exist_ok=True)
extraction=Path(tempfile.mkdtemp(prefix='portable_',dir=qa))
with zipfile.ZipFile(archive) as z:z.extractall(extraction)
report=extraction/'runtime.json'
env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen'
env['PATH']=str(Path(os.environ['WINDIR'])/'System32')
for key in ('PYTHONPATH','PYTHONHOME','VIRTUAL_ENV','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH'):env.pop(key,None)
subprocess.run([str(extraction/'VolumetroDesk'/'VolumetroDesk.exe'),'--self-test',str(report)],cwd=extraction,env=env,timeout=30,check=True)
data=json.loads(report.read_text(encoding='utf-8'))
assert data=={'servos':2,'sensores':['X','Y','Z'],'conexion_automatica':False},data
print(json.dumps({'zip':str(archive),'bytes':archive.stat().st_size,'runtime':data,'clean_extraction':str(extraction)},ensure_ascii=False))
