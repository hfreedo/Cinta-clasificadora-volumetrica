# Guía para explicar y demostrar el repositorio

[Portada](../README.md) · [Arquitectura](ARQUITECTURA.md) · [Pruebas de banco](PRUEBAS_DE_BANCO.md)

## Idea central

«Desarrollamos dos sistemas de automatización con Arduino UNO. Uno estima las dimensiones y el volumen exterior de un objeto; el otro decide su recorrido según el color. Se presentan juntos porque comparten una metodología de sensores, procesamiento y actuadores, pero cada uno funciona de manera independiente».

## Guion sugerido de 8–10 minutos

| Tiempo | Tema | Evidencia a mostrar |
|---|---|---|
| 1 min | Problema y organización modular | Portada y carpetas de los dos módulos |
| 2 min | Medición volumétrica | Sensores, esquina de apoyo, fórmulas y mapa X/Y/Z |
| 2 min | Cinta clasificadora | Flujo de estados, sensor de color y desvíos |
| 2–3 min | Demostración | Lecturas, calibración y un ciclo supervisado de cada sistema |
| 1–2 min | Resultados y límites | Mediciones registradas, pruebas de software y pruebas físicas pendientes |

## Explicar el volumétrico

1. Mostrar la esquina y los planos que se usan como referencia. Los sensores miden el espacio libre hasta la cara del objeto.
2. Escribir: **dimensión = referencia − distancia medida** y **volumen = largo × ancho × altura**.
3. Para un prisma con base 3×3 cm y altura 5 cm, explicar los valores teóricos: X=12,5 cm, Y=23 cm, Z=13 cm; el volumen exterior esperado es 45 cm³. Son valores de ejemplo, no una medición observada.
4. Mostrar lecturas en vivo con el objeto y sin él. Una lectura estable del fondo no demuestra que se esté detectando el prisma.
5. Explicar reposo, empuje, límites y EEPROM. Los ángulos guardados son órdenes al servo; no hay realimentación de posición ni fuerza.

## Explicar la cinta

1. Mostrar que IR1 anuncia la llegada del objeto. La cinta pausa para tomar la lectura de color.
2. Explicar que se comparan canales R/G/B con referencias tomadas en el montaje real.
3. Mostrar la decisión: rojo continúa; azul usa servo 2; verde y amarillo usan servo 1 en la versión actual.
4. Identificar el recorrido por tiempo y el retorno a reposo lógico. No describirlo como confirmación física de llegada.
5. Mostrar cómo cambia una lectura cuando cambia la iluminación y por qué hace falta calibrar.

## Demostración reproducible

- Preparar ambos montajes y usar su propio UNO si deben operar simultáneamente.
- Tener identificados puertos, versiones de firmware y alimentación. No abrir monitor serie y app sobre el mismo puerto.
- Comenzar con lecturas, después con un actuador a la vez y finalmente con un ciclo.
- Mostrar el efecto real de la parada en cada sistema: sus comportamientos no son iguales.
- Presentar una tabla de resultados reales y repetir la prueba; no reemplazar resultados por los valores esperados.

## Preguntas que conviene poder responder

| Pregunta | Punto que debe explicarse |
|---|---|
| ¿Por qué tres sensores? | Se necesita una distancia por dimensión del prisma. |
| ¿Mide el plástico o el hueco interior? | Se estima el volumen exterior a partir de dimensiones externas. |
| ¿Por qué EEPROM? | Mantiene referencias entre apagados; cada sistema usa un formato propio. |
| ¿Qué significa modular? | Cada proyecto conserva entradas, salidas, firmware y pruebas independientes. |
| ¿La app controla la cinta? | No. VolumetroDesk utiliza el protocolo del volumétrico. |
| ¿Compilar demuestra que funciona? | Solo verifica la construcción del programa; la mecánica y la medición necesitan pruebas físicas. |
| ¿Qué falta mejorar? | Registrar exactitud y repetibilidad, confirmar alcance de brazos y revisar el margen de RAM de la cinta. |

## Material de apoyo pendiente de aportar

Agregar fotografías del montaje real, esquema eléctrico final, dimensiones de la estructura, resultados medidos y enlaces a videos cuando existan. No se incluyen imágenes de ejemplo como si fueran evidencia del prototipo.
