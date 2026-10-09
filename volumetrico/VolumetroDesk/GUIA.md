# VolumetroDesk 1.0 — CELE

Aplicación de escritorio inspirada en ServoDesk del brazo robótico SCS, adaptada a este sistema volumétrico. No usa ni modifica la app o el protocolo del brazo robótico.

## Abrir

En la distribución portable, abrir `VolumetroDesk.exe` y conservar la carpeta `_internal` a su lado. No requiere Python instalado. Para ejecutar el código fuente: `python -m pip install -r requirements.txt` y después `python app.py`, o abrir `ABRIR.cmd` si las dependencias ya están instaladas.

1. Cargar **Volumetrico_UNO 1.1.0**, incluido en este proyecto. El test independiente y la versión 1.0.1 no tienen el protocolo completo de esta app.
2. Cerrar el monitor serie de Arduino y otras aplicaciones que ocupen el puerto.
3. Abrir la app, elegir COM y pulsar **Conectar USB**. Espera el reinicio normal del UNO y consulta su configuración; no acopla servos ni inicia movimientos.
4. Cuando se identifique el firmware, aparecen los controles habilitados según su estado. **Consultar Arduino** recupera la configuración después de un error. Nunca hay reconexión o reproducción automática.

## Mediciones y calibración

Mapeo actual sin cambiar pines: X=altura (D2/D3, 17,5 cm); Y=largo (D4/D5, 26 cm); Z=ancho (D6/D7, 16 cm). La app muestra la configuración real que responde el Arduino, no sustituye una EEPROM anterior automáticamente.

En **Mediciones**, introducir las referencias correctas y pulsar **Aplicar referencias (RAM)**. Después ir a **Servos → Guardar calibración en EEPROM**. Solo la respuesta `OK EEPROM guardada` confirma el guardado. Reiniciar y consultar para comprobar persistencia en tu placa.

**Ver distancias en vivo** muestra los tres sensores por turnos (aproximadamente 3,3 muestras/s por sensor), sin mediana y sin rechazar lecturas por superar las referencias. La lectura se pausa durante movimiento y vuelve al terminar. Los datos sin actualización se marcan como anteriores después de dos segundos. El registro incluye duración del eco, distancia y estado:

- ECO: eco recibido; no garantiza que pertenezca al prisma.
- SIN_ECO: no hay pulso completo en 30 ms.
- ECHO_ALTO: ECHO estaba alto antes de disparar.
- MAS_ALLA_REFERENCIA: lectura mayor que referencia +0,5 cm.
- FUERA_NOMINAL: fuera de 2–400 cm.

**Medir sin mover** requiere servos libres y obtiene dimensiones/volumen filtrados. **Alinear y medir** ejecuta el ciclo del firmware y requiere ambos servos acoplados con reposo y empuje definidos. Al final del ciclo se liberan. Los resultados también se muestran en el LCD. Durante VIVO el LCD indica el modo; las tres distancias crudas están en la app y el monitor serie.

## Ajustar servos

- Modificar deslizador, número o ±1° solo cambia el objetivo en pantalla. **Mover** envía la orden; se espera su terminación antes de enviar otra.
- **Acoplar** activa el servo en el ángulo objetivo. Puede saltar porque no conoce su posición física: empezar sin brazos o con espacio libre.
- **Liberar** desactiva pulsos. Con el servo libre, establecer mínimo/máximo y **Aplicar límites**. Esto invalida sus posiciones de reposo/empuje y hay que volver a fijarlas.
- Llegar a una posición con Mover y luego **Fijar reposo actual** o **Fijar empuje actual**. Se fija el ángulo ordenado al Arduino, no el objetivo del deslizador que aún no se haya enviado.
- **Guardar calibración en EEPROM** guarda referencias, límites y posiciones de ambos servos. **Recuperar EEPROM** reemplaza la configuración RAM y libera los servos.
- Los indicadores distinguen la configuración en RAM y la confirmación del guardado. Los ángulos son órdenes, no mediciones físicas.

## Diseñar movimientos

En **Diseñar movimiento**, añadir los dos objetivos actuales como paso; editar ángulos y pausa, reordenar y guardar/abrir JSON. La previsualización no envía comandos: muestra ángulos de dos brazos y no modela la posición real de los pivotes, colisiones, fuerza ni alcance.

**Ejecutar en Arduino** requiere ambos servos ya acoplados. Valida todos los pasos contra los límites confirmados antes de comenzar. Para cada paso mueve S1, espera su terminación; mueve S2, espera su terminación; cumple la pausa. No hay movimiento simultáneo ni bucle automático. Al terminar quedan acoplados en la última posición; usar Liberar o STOP cuando corresponda. La secuencia personalizada no reemplaza el ciclo fijo de Alinear y medir ni se almacena en EEPROM.

Los archivos JSON son diseños de la PC (máximo 100 pasos; pausas de 0–10000 ms). Abrirlos no mueve hardware ni modifica EEPROM. La aplicación funciona sin placa para crear y previsualizar estos archivos. Al reproducir, se ejecuta una copia del diseño; editar la tabla durante la reproducción no cambia la secuencia que ya comenzó.

## STOP y desconexión

STOP elimina las órdenes pendientes de la app, solicita detener el firmware y espera su confirmación. El Arduino libera ambos servos, cancela el ciclo y la lectura en vivo, y queda bloqueado hasta Reanudar. Reanudar no vuelve a acoplarlos ni reanuda la secuencia anterior.

STOP actúa por USB; no corta alimentación ni frena mecánicamente. Al cerrar/desconectar se intenta enviar STOP, pero no se garantiza recepción. Si se pierde el cable, el comando que ya estaba en el Arduino puede terminar: comprobar físicamente el equipo y cortar la fuente ante un atasco. La aplicación cancela la secuencia ante error o timeout; no reenvía movimientos automáticamente.

**Exportar registro** guarda las últimas 1000 líneas de comunicaciones (incluye lecturas y respuestas). Es útil para comparar prisma presente, objeto ausente y una placa plana a la misma distancia.

## Validación

Pruebas automatizadas de protocolo e interfaz con transporte simulado: ACK versus terminación de IR, STOP prioritario, cancelación ante error/timeout, validación de archivos/límites y guardado confirmado. Capturas de las tres pestañas revisadas mediante render Qt. Compilación UNO independiente de estas pruebas.

Pendiente: movimiento real, exactitud de sensores, alimentación, LCD y EEPROM física. Una prueba simulada no confirma aceptación por un Arduino real. Las capturas de QA usan respuestas simuladas, no representan una conexión física.
