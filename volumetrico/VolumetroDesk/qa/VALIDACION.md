# Validación — 6 de octubre de 2026

- 17 pruebas automatizadas aprobadas: `python -m unittest discover -s VolumetroDesk -p 'test_*.py'`.
- Cubren secuenciación, espera de fin de movimiento, prioridad de STOP, cancelación por error/timeout, validación de diseños y límites, telemetría, ausencia de movimientos al editar sliders y confirmación de EEPROM.
- Capturas `servos.png`, `mediciones.png`, `diseno.png`: render Qt revisado. Datos del transporte simulado; no conexión física. Se agregó desplazamiento en pestañas para pantallas de menor altura.
- Firmware 1.1.0 compilado con Arduino AVR Boards 1.8.8: 17.918 bytes flash (55%), 855 bytes RAM estática (41%).
- Portable Windows x64 generado con PyInstaller 6.19.0, Python 3.13.12, PySide6 6.10.2 y pySerial 3.5.
- ZIP final probado en extracción nueva (`portable_jvrwov_m`): salida 0, dos servos, tres sensores X/Y/Z, ninguna conexión automática. PATH reducido a System32, sin Python en PATH ni variables PYTHONHOME/PYTHONPATH. No se instalaron dependencias durante esta verificación.
- Se corrigió un conflicto de empaquetado: QtCore requiere los símbolos ICU de Windows; PyInstaller encontraba el ICU de Poppler en PATH. El archivo .spec excluye esas dos DLL ajenas. No se modificó la instalación de Poppler, Python ni Windows.
- No se abrió un puerto COM real, no se cargó el firmware ni se probaron movimientos físicos, LCD, exactitud, alimentación o EEPROM real. No probado en otra PC.

## Reconstrucción

Desde la raíz del repositorio reorganizado, conservar el .spec que contiene la corrección del empaquetado:

```powershell
python -m PyInstaller --noconfirm --distpath Entregas --workpath volumetrico\VolumetroDesk\build volumetrico\VolumetroDesk\VolumetroDesk.spec
python volumetrico\VolumetroDesk\package_portable.py
```

La guía y el firmware se copian al portable al empaquetar. El ZIP incluye toda la carpeta; no distribuir el EXE aislado.

