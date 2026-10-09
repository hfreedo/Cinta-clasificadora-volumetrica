# Mantener y ampliar los módulos

1. Identificar el módulo afectado. No mezclar pinouts o comandos entre proyectos.
2. Conservar los sketches de diagnóstico independientes del firmware principal.
3. Documentar cualquier cambio de pin, protocolo, EEPROM o comportamiento al arrancar/detener.
4. Compilar el sketch afectado para `arduino:avr:uno` y revisar RAM, no solo Flash.
5. Para la app, ejecutar desde la raíz:

```sh
python -m unittest discover -s volumetrico/VolumetroDesk -p 'test_*.py' -v
```

6. Indicar si la comprobación fue de compilación, simulación, interfaz o hardware real. Adjuntar evidencia cuando exista.
7. No subir ejecutables, paquetes, entornos Python, credenciales ni registros personales al código fuente. Distribuir binarios mediante Releases si se decide publicar una entrega.

Para reconstruir el portable Windows, usar el archivo `.spec` existente: incluye la corrección de bibliotecas ICU. Desde la raíz:

```powershell
python -m pip install -r volumetrico/VolumetroDesk/requirements.txt
python -m pip install pyinstaller
python -m PyInstaller --noconfirm --distpath Entregas --workpath volumetrico/VolumetroDesk/build volumetrico/VolumetroDesk/VolumetroDesk.spec
python volumetrico/VolumetroDesk/package_portable.py
```

El script de empaquetado verifica una extracción nueva sin Python en PATH. No conecta hardware.
