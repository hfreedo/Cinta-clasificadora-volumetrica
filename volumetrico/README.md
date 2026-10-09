# CELE — Sistema volumétrico, firmware 1.1.0

Arduino UNO, tres HC-SR04, dos SG90 y LCD I²C 16×2. Archivo a abrir en Arduino IDE: `Volumetrico_UNO/Volumetrico_UNO.ino`.

[Volver a la portada del repositorio](../README.md)

Los comandos relativos de esta guía se ejecutan desde la carpeta `volumetrico/`. El módulo funciona de forma independiente de la cinta.

## App de escritorio

VolumetroDesk permite ajustar los servos, definir secuencias de movimientos, observar los sensores y calibrar/guardar EEPROM desde una ventana. Abrir `../Entregas/VolumetroDesk/VolumetroDesk.exe` (distribución local, fuera del historial Git). Leer `VolumetroDesk/GUIA.md`. Requiere cargar este firmware 1.1.0; el test independiente no es compatible con la app. Código fuente: `VolumetroDesk/app.py`.

El firmware incorpora `VIVO ON` y `VIVO OFF`: distancias sin filtro, un sensor cada 100 ms, timeout de 30 ms; se pausa durante los movimientos. `SENSOR X ECO_US=... CM=... STATUS=...` permite distinguir eco ausente, ECHO alto y distancia fuera de referencia. STOP también cancela VIVO. El mapa y formato EEPROM anteriores se conservan.

## Conexiones

| Componente | UNO | Alimentación |
|---|---|---|
| HC-SR04 X, altura, referencia 17,5 cm | TRIG D2, ECHO D3 | VCC 5 V UNO, GND común |
| HC-SR04 Y, lado largo, referencia 26 cm | TRIG D4, ECHO D5 | VCC 5 V UNO, GND común |
| HC-SR04 Z, lado ancho, referencia 16 cm | TRIG D6, ECHO D7 | VCC 5 V UNO, GND común |
| Servo 1, brazo 12 cm | Señal D8 | Fuente externa regulada 5 V |
| Servo 2, brazo 6,5 cm | Señal D9 | Fuente externa regulada 5 V |
| LCD I²C 16×2 | SDA A4, SCL A5 | VCC 5 V UNO, GND común |

Alimentar el UNO por USB y los servos directamente desde la fuente externa de 5 V prevista (1,5–2 A, capacidad pendiente de confirmar en carga). Unir negativo de la fuente, GND del UNO y GND de todos los componentes. No unir el positivo externo al pin 5 V del UNO en este montaje. No alimentar los servos desde el regulador del UNO. Cortar alimentación antes de modificar cableado.

Los sensores deben apuntar perpendicularmente a las caras del prisma; las referencias se miden sobre sus ejes hasta los planos de apoyo, no diagonalmente al vértice. La esquina necesita topes físicos de referencia para una alineación repetible; el contorno interior del dibujo solo delimita el espacio. Las paletas, topes y paredes no deben interceptar los haces. El sensor superior debe estar sobre el objeto, no sobre el centro de toda la plataforma si el objeto no está allí.

## Compilación y carga

Dependencias verificadas: Arduino AVR Boards 1.8.8, Servo 1.3.0 y LiquidCrystal I2C 1.1.2 (clase `LiquidCrystal_I2C`). Wire y EEPROM vienen con el core AVR. Se comprueban las direcciones LCD 0x27 y 0x3F; otro módulo requiere adaptar las direcciones y, si corresponde, su biblioteca. Ajustar el potenciómetro de contraste si enciende sin mostrar texto.

Seleccionar Arduino UNO y su puerto en Arduino IDE; cargar el sketch. Monitor serie a **115200 baudios**, con nueva línea o ambos NL/CR. El firmware no fue cargado en una placa durante su preparación.

Compilación reproducible:

```powershell
arduino-cli compile --fqbn arduino:avr:uno --warnings all Volumetrico_UNO
```

## Actualización 1.0.1: canales reales

Sin cambiar pines: X mide altura (17,5 cm); Y mide largo (26 cm); Z mide ancho (16 cm). LCD y RESULTADO usan esta correspondencia. DISTANCIAS sigue en orden X/Y/Z.

Una EEPROM válida conserva los valores anteriores al cargar el nuevo sketch. Para actualizar solo las referencias y conservar posiciones de servos, enviar al firmware principal, estando inactivo: `CAL X 17.5`, `CAL Y 26.0`, `CAL Z 16.0`, `GUARDAR` y `CAL VER`, uno por línea. No usar RESTABLECER para esta actualización. Reiniciar y consultar CAL VER para comprobar persistencia. No se cambia el formato EEPROM ni se sobrescriben datos automáticamente.

## Primera prueba: sensores, sin mover brazos

Al arrancar, no se acoplan los servos. Las referencias iniciales son 17,5 / 26 / 16 cm (X / Y / Z). La EEPROM vacía o inválida no habilita posiciones de servo.

Enviar un comando por línea, usando punto decimal:

```text
CAL X 17.5
CAL Y 26.0
CAL Z 16.0
CAL VER
GUARDAR
LEER
```

`LEER` requiere ambos servos libres. Colocar manualmente el prisma contra los topes, con base 3×3 cm y altura 5 cm. Distancias teóricas X/Y/Z: 12,5 / 23 / 13 cm. Dimensiones esperadas largo/ancho/altura: 3 / 3 / 5 cm; volumen exterior esperado: 45 cm³. No son resultados de una prueba física.

Se toman cinco lecturas por eje, separando todos los disparos al menos 70 ms. Se usa la mediana y se rechaza una dispersión superior a 0,5 cm, eco ausente, distancia menor de 2 cm, distancia mayor que la referencia +0,5 cm o dimensión menor de 0,5 cm. El timeout de cada eco es 12 ms. La adquisición completa demora aproximadamente 1,1 s. No se muestra un volumen nuevo si falla un eje; el LCD muestra error y el monitor da el motivo.

La estabilidad no demuestra que el eco corresponda al objeto: un fondo puede producir lecturas estables. El prisma de 3×3×5 cm es pequeño respecto al haz; validar primero detección y repetibilidad. Si no es fiable, habrá que cambiar la geometría o el sensor. El umbral de dispersión no es una garantía de exactitud de ±0,5 cm.

## Calibración de servos

**El primer ACOPLAR puede producir un salto:** el SG90 no informa su posición real. Hacerlo inicialmente sin los palitos instalados o con el área despejada. Los límites iniciales 20–160° son provisionales y no garantizan ausencia de choques. El software solo controla ángulos ordenados.

1. Con el servo libre, ajustar si hace falta sus límites: `SERVO 1 LIMITES 60 120`. Los números son ilustrativos; elegirlos según el montaje. Cambiar límites invalida las dos posiciones guardadas de ese servo en RAM.
2. Ejecutar `SERVO 1 ACOPLAR 90`, esperar a que termine físicamente y montar el brazo en una orientación que no fuerce el mecanismo.
3. Ajustar gradualmente con `SERVO 1 IR 92`, luego otros pequeños pasos según corresponda. `IR` avanza a 1° cada 20 ms. Esperar `OK movimiento terminado (orden)` antes del siguiente comando.
4. Cuando esté apartado del haz y del objeto: `SERVO 1 FIJAR REPOSO`.
5. Ajustar hasta un contacto suave de alineación, sin mantener el servo bloqueado: `SERVO 1 FIJAR EMPUJE`.
6. Volver al ángulo de reposo con `IR`. Repetir con `SERVO 2 ...`.
7. Ejecutar `CONFIG VER` y `GUARDAR`.

No copiar ángulos de empuje arbitrarios. Comprobar que los brazos de 12 y 6,5 cm realmente alcancen este prisma. Si un brazo gira sin llegar o arrastra el objeto al retirarse, corregir el montaje; el firmware no puede compensar un alcance insuficiente.

## Ciclo automático

Se habilita cuando ambos servos tienen REPOSO y EMPUJE distintos y calibrados (FLAGS 3), están acoplados y STOP no está activo. Tras encender, ejecutar `SERVO 1 ACOPLAR <reposo1>` y `SERVO 2 ACOPLAR <reposo2>` con los ángulos reales consultados en `CONFIG VER`.

`MEDIR` realiza: ir a reposo → empujar servo 1 → empujar servo 2 mientras el primero mantiene su orden → retirar ambos a reposo → desactivar pulsos → esperar 800 ms → medir X/Y/Z → presentar resultado. Se mantiene el orden 1 luego 2 en esta primera versión; comprobar que sea adecuado para el montaje.

Al finalizar quedan libres; es necesario volver a acoplarlos para otro ciclo. Este comportamiento también obliga a reconocer explícitamente la primera posición después de liberar los motores. Retirar los brazos puede desplazar el objeto: verificar que siga contra los topes antes de aceptar resultados.

LCD alterna largo/ancho y altura/volumen cada tres segundos. El monitor emite `RESULTADO L=... A=... H=... cm V=... cm3` y `DISTANCIAS ...`, para facilitar una interfaz futura.

## Comandos

| Comando | Efecto |
|---|---|
| `AYUDA` | Resumen de comandos |
| `ESTADO`, `CONFIG VER`, `CAL VER` | Configuración, acoplamiento y estado |
| `CAL X 17.5` / `CAL Y 26.0` / `CAL Z 16.0` | Referencia en RAM, rango 4–100 cm |
| `GUARDAR`, `CAL GUARDAR` | Guarda toda la configuración en EEPROM |
| `CARGAR` | Recupera configuración válida y libera servos |
| `RESTABLECER` | Valores iniciales en RAM, sin borrar EEPROM hasta GUARDAR |
| `SERVO n LIMITES min max` | Límites 0–180°, servo libre; invalida posiciones |
| `SERVO n ACOPLAR angulo` | Activa servo libre en ese ángulo |
| `SERVO n IR angulo` | Movimiento gradual dentro de límites |
| `SERVO n FIJAR REPOSO` / `SERVO n FIJAR EMPUJE` | Memoriza ángulo ordenado actual en RAM |
| `SERVO n LIBERAR` | Desactiva pulsos del servo en reposo lógico |
| `LEER` | Mide sin mover; requiere servos libres |
| `MEDIR` | Ciclo automático con calibración previa |
| `STOP` | Cancela ciclo y pulsos; bloquea nuevos movimientos/lecturas |
| `REANUDAR` | Quita bloqueo STOP, sin acoplar ni mover |

Durante un ciclo solo se aceptan STOP y consultas/AYUDA; el resto devuelve ocupado. En los comandos, `n` significa 1 o 2. Enviar una línea y esperar respuesta. No enviar un flujo continuo de texto: STOP viaja por el mismo puerto y no es una parada física de emergencia.

STOP no lleva los brazos a reposo, no frena mecánicamente ni desconecta la fuente. Su bloqueo dura hasta REANUDAR o reinicio. Tras un reinicio siempre se arranca sin pulsos ni ciclo automático. Si hay atasco, cortar la alimentación externa. Los SG90 no confirman contacto, alineación ni posición alcanzada.

EEPROM: firma, versión, referencias, límites y posiciones con indicadores de calibración y CRC16. Solo GUARDAR escribe, mediante EEPROM.put (actualiza bytes modificados). No guarda cada paso ni cada medición. Una escritura interrumpida puede invalidar la configuración: al arrancar se rechaza y se vuelve a valores iniciales; no hay copia de respaldo. CARGAR rechaza datos inválidos sin sustituir la configuración vigente.

## Verificación de banco pendiente

1. Encender sin brazos: confirmar LCD y ausencia de movimiento automático.
2. Guardar referencias y posiciones; apagar/encender y comprobar recuperación con CONFIG VER y servos libres.
3. Comprobar que MEDIR se rechaza antes de calibrar/acoplar ambos servos.
4. Probar STOP durante IR y MEDIR: cancelación de pulsos y rechazo de nuevos movimientos hasta REANUDAR. Confirmar físicamente la respuesta.
5. Probar objeto ausente, eco bloqueado e inestabilidad; comprobar que no persiste un volumen anterior en pantalla.
6. Hacer diez mediciones del prisma colocado manualmente, registrar dimensiones y volumen y compararlos con medición independiente. No se ha acordado aún tolerancia final.
7. Verificar alcance, deslizamiento, retirada sin arrastre y alineación. Repetir medidas después del ciclo automático; vigilar caídas de alimentación y reinicios.

## Evidencia de preparación

Compilación para UNO exitosa el 6 de octubre de 2026: 17.918 bytes flash (55%), 855 bytes RAM estática (41%). Versión 1.1.0 compilada sin errores; test actualizado: 9.050 bytes flash y 566 bytes RAM. No se realizaron pruebas físicas, de exactitud, potencia, LCD, EEPROM real o alineación; tampoco se cargó el firmware en hardware.

Referencias: [EEPROM de Arduino](https://docs.arduino.cc/learn/built-in-libraries/eeprom/) y [hoja de datos HC-SR04](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf). La hoja indica mínimo 2 cm y recomienda ciclos mayores de 60 ms; la precisión nominal no garantiza detección de este objeto pequeño ni exactitud volumétrica.




