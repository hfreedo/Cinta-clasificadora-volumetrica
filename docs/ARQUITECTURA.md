# Arquitectura modular

[Portada](../README.md)

La unidad de organización es el **sistema físico**, no la placa compartida como modelo. Cada módulo contiene un firmware principal, un programa de diagnóstico y su documentación. La aplicación de PC pertenece al volumétrico.

| Contrato | Volumétrico | Cinta |
|---|---|---|
| Entrada principal | Distancias en tres ejes | Evento IR1 y canales de color |
| Transformación | Referencia menos distancia; producto de dimensiones | Comparación con referencias de color |
| Salidas | Dimensiones, volumen, alineación | Desvío, movimiento de cinta, contadores |
| Controlador | UNO con firmware 1.1.0 | UNO con firmware 2.0 |
| Comunicación | Texto a 115200; app VolumetroDesk | Texto y JSON a 115200; monitor serie |
| Persistencia | Referencias y servos; GUARDAR explícito | Referencias de color; guardado por calibración |
| Arranque de servos | Libres, sin movimiento automático | Acoplados, con orden a reposo |
| Parada | STOP libera servos y bloquea órdenes | stop detiene motor/ciclo; servos siguen acoplados |

## Separación de responsabilidades

### Volumétrico

- `Volumetrico_UNO.ino`: validación de comandos, movimiento, adquisición, cálculo, LCD y EEPROM.
- `Test_Ultrasonicos_Vivo.ino`: observación de ecos sin los filtros del cálculo volumétrico.
- `VolumetroDesk/protocol.py`: orden pendiente, confirmación, fin de movimiento, timeout y validación de secuencias.
- `VolumetroDesk/app.py`: interfaz y transporte USB. Los diseños JSON se guardan en PC; no reemplazan el ciclo fijo del firmware.

Mapa lógico confirmado: **X = altura 17,5 cm; Y = largo 26 cm; Z = ancho 16 cm**, manteniendo los pines. Las dimensiones se obtienen restando cada distancia a su referencia; el volumen se calcula solo después de validar las lecturas.

### Cinta

- `conveyor.ino`: máquina de estados, comandos, decisión de color, motor, desvíos y telemetría.
- `conveyor_test.ino`: consulta y accionamiento de componentes, calibración de referencias de color.
- La temporización hace visible cada etapa de la demostración. Un final por tiempo no constituye confirmación física de llegada a la salida.

## Independencia y futuras integraciones

Se puede compilar y usar cada módulo por separado. No hay importaciones entre firmwares ni sincronización entre dos placas. No se comparte EEPROM, pinout ni protocolo pese a usar la misma velocidad serie.

Una futura línea que mida y luego clasifique necesitaría definir quién inicia cada etapa, identificador del objeto, acuses de recibo, tiempos de espera y parada de ambos equipos. Eso es una extensión propuesta, **no una función implementada** en este repositorio.
