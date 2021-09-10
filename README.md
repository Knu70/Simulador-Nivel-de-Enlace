# Simulador Nivel de Enlace

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/ComunicacionEntreHosts.png" width="500" />
</div>

### Ejercicio 1: Ejecutar el protocolo 2 con la siguiente orden ./protocol2 100 0 0 0. Dibujar el resultado obtenido.

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio1.png" width="500" />
</div>

### Ejercicio 2: Ejecutar el protocolo 3 con diferentes tasas de pérdida de paquetes, diferentes tasas de errores en trama. ¿Cómo afectan el incremento de la pérdida de paquetes a la eficiencia del protocolo 3? ¿Cómo afecta el incremento de la tasa de errores en tramas a la eficiencia del protocolo 3?

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio2.png" width="500" />
</div>

Todas las ejecuciones las he realizado con un tiempo de simulación y un time out iguales cambiando
el porcentaje de tramas que se pierden y el porcentaje de tramas que llegan con error para
estudiar como varia la eficiencia del protocolo 3.

Se puede observar, como era de esperar, cómo en las ejecuciones 2, 3, 4, y 5, en las que sólo se
añade porcentaje de tramas que se pierden, la eficiencia baja conforme sube el porcentaje. Esto
también ocurre en las ejecuciones 6, 7, 8, 9 y 10 cuando sólo vario el porcentaje de tramas que
llegan con error, pero esta vez, la eficiencia baja considerablemente más que cuando sólo se varia el
porcentaje de tramas que se pierden.

En las ejecuciones 11, 12, 13 y 14 añado porcentajes tanto de tramas que se pierden como de
tramas que llegan con error, y de nuevo, como era de esperar, la eficiencia baja más que en los
casos anteriores.

Hay que destacar que a partir del 30% de tramas que se pierden, la eficiencia no baja de 18% hasta
que llega a 0% (cuando se supera el 90% de tramas perdidas); esto también pasa a partir del 50%
de tramas que llegan con error, en las que la eficiencia no baja del 6% hasta que llega a 0%. Resalto
esto, porque, aunque en las ejecuciones 11, 12 y 13 las eficiencias siempre son menores que
cuando sólo se añaden un porcentaje de tramas que se pierden o con llegada con error, pero,
cuando hablamos del mínimo de eficiencia antes de llegar a al 0% en la trama 15, con un 10%, esta
es un porcentaje intermedio entre los 18% de las tramas que se pierden y el 6% de las tramas que
llegan con error.

### Ejercicio 3: Ejecutar el protocolo 3 con la siguiente orden: ./protocol3 100 10 10 10 0. Dibujar
en la gráfica inferior el resultado obtenido. Interpretar y comentar cada una de las
situaciones que se producen en la comunicación y sus causas (errores, timeouts,
retransmisiones, etc.).

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.1.png" width="500" />
</div>
<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.2.png" width="500" />
</div>

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.3.png" width="500" />
</div>
<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.4.png" width="500" />
</div>
<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.5.png" width="500" />
</div>
<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio3.6.png" width="500" />
</div>

### Ejercicio 4: Ejecutar el protocolo 3 con la siguiente orden: ./protocol3 100 40 10 10 0. Dibujar en una gráfica como la anterior el resultado obtenido. ¿cuál es la diferencia con el apartado 3?.

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio4.1.png" width="500" />
</div>

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio4.2.png" width="500" />
</div>
<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio4.3.png" width="500" />
</div>

En esta ejecución del protocolo 3 se aumenta el Time Out de 10 a 40 respecto al ejercicio anterior y se puede observar como ahora los mensajes de confirmación ack llegan de forma correcta al contrario que en el ejercicio 3 donde a veces no daba tiempo a que llegase el ack y saltaba el Time Out. Esto hacía que no hubiese una sincronización entre emisor y receptor y terminaban llegando fuera de tiempo sin saber cuál se confirmaba. Esto hace que la eficiencia haya aumentado de 38% a 40%. 

### Ejercicio 5: Ejecutar el protocolo 4. Indicar si aporta ventajas con respecto al protocolo 3. En términos de la eficiencia utilizada por el simulador, ¿resulta más adecuado utilizar el protocolo 4 o el protocolo 3 para un enlace de red?

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio5.png" width="500" />
</div>

En términos de eficiencia el protocolo 3 es más adecuado para un enlace de red. Cuando el
porcentaje de tramas que se pierden y llegan con error es 0, el protocolo 3 tiene una eficiencia del
99% y el protocolo 4 del 49%, pero esto no es todo; en el protocolo 3 cuento más se sube el
porcentaje de tramas que se pierden o que llegadas con error, la eficiencia baja de forma de
gradual, lo que facilita la estabilidad entre la conexión Emisor-Receptor, en cambio, en el protocolo
4 con un mayor porcentaje de tramas que se pierden o que llegan con error, puede tener una
eficiencia mayor que con un porcentaje menor.

También se observa en el protocolo 4 que a mayor porcentaje de tramas que se pierden o que
llegan con error, mayor eficiencia tiene. Esto no es productivo porque la eficiencia resultante es
mayor que en protocolo 3 cuantas más tramas se pierdan o lleguen con error.

### Ejercicio 6: Ejecutar los protocolos de ventana deslizante (protocolo5 y protocolo6) y realizar
una comparación entre ellos. Para ello probar la ejecución de los 2 protocolos bajo las
mismas circunstancias e ir variando los parámetros de entrada. ¿Bajo qué condiciones
resulta más adecuado utilizar el protocolo5? ¿y el protocolo6?

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio6.1.png" width="500" />
</div>

<div align="center">
  <img src="https://github.com/road2root/Simulador-Nivel-de-Enlace/blob/main/Images/Ejercicio6.2.png" width="500" />
</div>

El protocolo 6 es más eficiente en cada prueba de ejecución que el protocolo 5, esto se debe que
el protocolo 6 usa la técnica de rechazo selectivo (RS), disponiendo de un buffer de memoria y
pudiendo almacenar tramas correctas y que lleguen con error. El protocolo 5, en cambio, usa la
técnica de rechazo no selectivo(NRS), en la que no hay un buffer de memoria y si ocurre un error
en una trama, el emisor tendrá que retransmitir esa trama y todas las tramas siguientes que hubiese
enviado.

Respondiendo a la pregunta que bajo qué condiciones sería más adecuado utilizar el protocolo 5,
seria cuando el receptor no tenga memoria para almacenar las tramas correctas después de una
con error.
