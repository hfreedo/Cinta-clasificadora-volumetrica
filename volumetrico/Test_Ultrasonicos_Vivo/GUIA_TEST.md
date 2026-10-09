# Test en vivo de los tres HC-SR04

Este sketch sustituye temporalmente al firmware de medición en la placa. El archivo original se conserva y este test no modifica la EEPROM. No mueve los servos. Para aislar la prueba, desconectar la alimentación de los servos y apartar manualmente los brazos con la alimentación cortada.

## Uso

1. Abrir `Test_Ultrasonicos_Vivo.ino` en Arduino IDE y cargarlo en Arduino UNO.
2. Abrir el monitor serie a **115200 baudios**. Inicia mediciones automáticamente. Admite cualquier ajuste de fin de línea.
3. Mantener el mismo cableado: X altura TRIG D2/ECHO D3, Y largo D4/D5, Z ancho D6/D7; LCD SDA A4/SCL A5. Alimentación de sensores a 5 V y GND común.
4. Enviar una sola letra según la prueba:

| Letra | Acción |
|---|---|
| `T` | Tres sensores por turnos; cada sensor se actualiza aproximadamente cada 300 ms |
| `X`, `Y`, `Z` | Solo ese sensor, aproximadamente cada 100 ms; los otros no disparan |
| `P` | Pausa |
| `C` | Continúa |
| `R` | Reinicia mínimos, máximos y contadores |
| `?` | Ayuda |

Cada cambio de sensor/modo reinicia las estadísticas. No usar comandos del firmware principal: este test interpreta letras individuales.

## Lectura del monitor

Columnas CSV:

```text
ms,eje,eco_us,distancia_cm,estado,min_cm,max_cm,ecos,timeouts,echo_alto
```

Ejemplo ilustrativo, no resultado medido:

```text
1200,Y,1334,23.00,ECO,22.90,23.10,4,0,0
```

Se muestran la duración del pulso y la distancia sin mediana ni límites del espacio calibrado. Mínimo y máximo incluyen todos los ecos recibidos desde R; no son ventanas móviles. `ecos` no significa mediciones físicamente correctas.

- `ECO`: pulso recibido, dentro del rango nominal y no más allá de la referencia +0,5 cm. No demuestra que provenga del objeto.
- `MAS_ALLA_REFERENCIA`: distancia mayor que 18 cm en X, 26,5 cm en Y o 16,5 cm en Z. Se muestra sin rechazarla. Las referencias de este test son fijas y no se leen de EEPROM.
- `SIN_ECO_30ms`: no se obtuvo un pulso completo antes del timeout. `NA` no significa distancia cero.
- `ECHO_ALTO_ANTES`: el pin ECHO ya estaba alto antes de disparar; no se dispara ese sensor en ese turno. Revisar cableado, módulo y alimentación.
- `FUERA_RANGO_NOMINAL`: distancia calculada menor de 2 cm o mayor de 400 cm. Se conserva el dato, pero no es una medición dentro de la especificación nominal.

LCD: distancias en cm; `SIN` = timeout, `ALTO` = ECHO alto antes del disparo, `---` = aún no medido. La salida completa está en el monitor serie.

## Comparación controlada

1. Elegir `X` y dejar fijo el sensor. Colocar el prisma manualmente en la posición final; ejecutar R y observar 10 segundos. Copiar las líneas.
2. Retirar el prisma, ejecutar R y observar otros 10 segundos: comparar con el fondo.
3. Colocar una placa plana rígida más grande, perpendicular al sensor, con su cara frontal exactamente en el mismo plano que la cara del prisma. Ejecutar R y comparar. No pegarla primero al prisma: su tamaño y orientación son variables que interesa aislar.
4. Repetir para Y y Z; luego repetir en T. Si individualmente funciona y en T empeora, investigar interferencias acústicas/reflexiones del montaje.
5. Con el prisma fijo, cambiar levemente su orientación y después devolverlo a la posición original. Registrar si desaparece el eco o salta hacia el fondo.

Para base 3×3 cm y altura 5 cm, las distancias teóricas son X=12,5 cm, Y=23 cm, Z=13 cm. Medirlas también con regla desde la referencia del sensor. Si la orientación del prisma cambia, cambian estos valores.

Si una placa grande es estable y el prisma no, el resultado apunta a tamaño de cara, orientación, geometría del haz o superficie; no permite culpar únicamente al plástico hueco. Si también falla la placa, revisar primero alimentación, conexiones, orientación y sensor. Si no aparece diferencia entre objeto presente y ausente, probablemente no se está detectando su cara de forma fiable.

El firmware principal agrupa bajo “sin eco o fuera de rango” tres situaciones: timeout de 12 ms, distancia menor de 2 cm y distancia mayor que referencia +0,5 cm. Este test las separa y amplía el timeout a 30 ms.

Volver a cargar `Volumetrico_UNO.ino` al terminar; la configuración anterior en EEPROM se conserva. Ninguna lectura de este diagnóstico demuestra por sí sola exactitud volumétrica.

