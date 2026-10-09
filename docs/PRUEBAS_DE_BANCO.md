# Pruebas de banco y registro de evidencias

[Portada](../README.md)

## Evidencia de software

El 8 de octubre de 2026 compilaron los cuatro sketches para UNO y pasaron las 17 pruebas de VolumetroDesk. Las pruebas usan transporte simulado; no abren un COM ni validan la respuesta física de un servo. GitHub Actions ejecuta compilación y pruebas sobre el código del repositorio; consultar cada ejecución para conocer su resultado real.

Los archivos `.ino` se reorganizaron sin cambios de contenido. No se modificaron pines, temporizaciones ni calibraciones para preparar el repositorio.

## Ensayo volumétrico

1. Medir con regla las referencias reales X/Y/Z, aplicar y guardar. Reiniciar y comprobar recuperación.
2. Con objeto colocado manualmente, registrar diez lecturas y compararlas con dimensiones medidas por un instrumento independiente.
3. Comparar prisma, fondo sin prisma y superficie plana mayor a igual distancia.
4. Calibrar cada brazo; comprobar alcance, ausencia de choques y que la retirada no arrastre el objeto.
5. Repetir las mediciones después del ciclo automático. Probar STOP durante desplazamiento y confirmar la desactivación física.

| Fecha / ensayo | Referencias X/Y/Z | Distancias X/Y/Z | Largo / ancho / altura | Volumen | Valor de comparación | Observación |
|---|---|---|---|---|---|---|
| Pendiente | — | — | — | — | — | No completado en esta entrega |

Elegir una tolerancia antes del ensayo. El filtro de dispersión de 0,5 cm no certifica exactitud de ±0,5 cm; la multiplicación puede amplificar el error relativo en objetos pequeños.

## Ensayo de cinta

1. Comprobar IR1, color, LCD, motor y servos individualmente. Documentar el cableado S0/S1 del sensor de color y la fuente real.
2. Calibrar rojo, verde, azul y amarillo, uno por uno, bajo iluminación constante.
3. Ejecutar al menos diez ciclos por color y contar aciertos, confusiones y atascos.
4. Confirmar la salida física de cada objeto y ajustar recorrido/velocidad conforme al montaje.
5. Ejecutar una prueba prolongada, revisar reinicios y comportamiento con color desconocido; el principal tiene 485 bytes de RAM libres para pila y variables locales.
6. Probar `stop`: debe detener el motor/ciclo; los servos no se liberan con esta orden. Comprobar que el comportamiento sea aceptable para la mecánica.

| Fecha | Color real | Lecturas R/G/B | Color detectado | Desvío observado | Tiempo / alimentación | Resultado |
|---|---|---|---|---|---|---|
| Pendiente | — | — | — | — | — | No completado en esta entrega |

## Incidencias conocidas y alcance

- Ambos sistemas carecen de medición de fuerza/posición real del servo.
- La parada por USB requiere que el enlace siga funcionando; no equivale a cortar la energía.
- Usar fuentes apropiadas para actuadores y masa común; verificar caídas de tensión bajo carga.
- El código de cinta acopla servos al arrancar. El volumétrico arranca con servos libres.
- El test de cinta comparte las referencias de color con su principal; el formato no es compatible con el volumétrico.
- La interfaz de la cinta no forma parte de esta entrega; su operación documentada es por monitor serie.
