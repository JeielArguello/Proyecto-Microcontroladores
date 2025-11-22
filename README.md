# Proyecto Microcontroladores - Telemetría Live For Speed a Custom HID y simulacion de 
volante, acelerador y freno con STM32F411E-DISCOVERY

## Introducción

En este proyecto desarrollamos un Custom HID con una placa de desarrollo STM32F411E-DISCOVERY. 
para conectarse a traves de la interfas USB HID que tienen los sistemas operativos en general
y poder simular un volante con acelerador y freno usando potenciometros rotativos, ademas
aprovechando la conexion USB HID, podemos obtener telemetría de Live For Speed u otro simulador 
y enviarla por esta interfaz al microcontrolador para luego mostrar mediante un lcd la 
informacion obtenida de la telemetría.


## Requerimientos para compilar el programa de telemetría en C

Requiere:
** Tener instalado Live For Speed o F1 24. Se puede descargar desde https://www.lfs.net/downloads.

** Instalar drivers de ST-LINK para que la pc reconozca el dispositivo Custom HID.
Se pueden descargar desde https://www.st.com/en/development-tools/stsw-link009.html.

** Configurar Live For Speed para que emita telemetría UDP. Esto se hace desde el 
archivo cfg.txt ubicado en la carpeta de instalación de Live For Speed. 
Agregar o modificar las siguientes líneas:
```
OutGauge Mode 1
OutGauge Delay 5
OutGauge IP 127.0.0.1
OutGauge Port 30000
OutGauge ID 1
```
Luego setear al archivo como de solo lectura para evitar que LFS lo sobreescriba.

** Sino se tiene instalado un compilador de C, descargar WinLibs desde 
https://winlibs.com/.

** Comando para compilar:

```bash
gcc LFSOTelemetry.c -Iinclude -Lx64 -lhidapi -lws2_32 -o Telemetria.exe
```

** Flashear el firmware en la placa STM32F411E-DISCOVERY usando STM32CubeIde.


## Explicación del funcionamiento del firmware en el microcontrolador para enviar datos por USB HID

El firmware del microcontrolador STM32F411E-DISCOVERY está desarrollado utilizando
STM32CubeIDE y las librerias STM32CubeF4, donde principalmente se utilizo la libreria
de USB Device. Luego se utilizaron las librerias de HAL para ADC y DMA, sumando la 
libreria de LCD_Groove provista por la catedra.

El firmware configura el microcontrolador para que funcione como un dispositivo Custom USB HID.
Se configuran 3 canales de un ADC para leer los valores de los potenciometros que simulan
el volante, acelerador y freno. Estos valores se leen mediante DMA. Esto quiere decir que
el microcontrolador puede leer los valores de los ADC sin necesidad de que el CPU intervenga, ya que 
la lectura y la escritura en registros es manejada por el controlador de DMA, para luego obtener 
las conversiones del ADC en un buffer de memoria. De esta manera, el CPU puede dedicarse a otras tareas
lo que permite hacer una obtencion de datos no bloqueante, mejorando la eficiencia del sistema.
Cabe mencionar que el ADC se configura para que realice conversiones continuamente, y el DMA
se configura en modo circular, lo que significa que una vez que se completan las conversiones, el DMA
guarda los nuevos valores en buffers y luego como los potenciometros daban demasiado ruido, 
se promediaban para obtener un valor más estable.

La parte compleja del firmware es el armado de toda la estructura de datos que se necesita
para lograr una conexion USB HID. Lo primero es notar que para usar esta interfas USB necesitamos 
una frecuencia de reloj de 48MHz, por lo que se configura el oscilador de ceramica del microcontrolador
para obtener dicha frecuencia. Luego se configura el modulo USB Device para que funcione como un
dispositivo Custom HID. Una vez configurado el modulo USB, CubeIde nos permite utlizar las funciones de 
las librerias USB HID donde se debe definir un descriptor HID que es una estructura de datos que define las caracteristicas
del dispositivo HID, como por ejemplo los reportes de entrada y salida, el tipo de uso, los
tamaños de los reportes y los tipos de datos que se van a enviar y recibir. En este caso, se define
un reporte de entrada que contiene los valores de los potenciometros (volante, acelerador y freno)
usados como ejes de un dispositivo joystick y un reporte de salida que puede ser utilizado
para enviar datos desde la PC al microcontrolador. Luego, cuando nuestro dispositivo Custom HID 
tiene definido su descriptor HID, teniendo asi definido los tamaños y tipos de reportes,
podemos tomar las conversiones del ADC y enviarlas a la PC mediante estos reportes de entrada. Para 
esto se utiliza la función `USBD_HID_SendReport()` que se encarga de enviar los datos al host
(PC) a través de la interfas USB HID. Finalmente, una vez que el firmware está cargado
en el microcontrolador,podemos conectarlo a una PC y el sistema operativo lo reconocerá
como un dispositivo HID genérico. Logrando poder vizualizarlo y poder comprobar su funcionamiento
con algun software de prueba de dispositivos HID como por ejemplo en windows "Game Controllers".

## Explicación del funcionamiento del firmware para obtener datos por USB HID

Para obtener datos de un Host (PC) hacia nuestro sistema embebido, se penso en usar los protocolos
mostrados en clases, pero para simplificar el desarrollo se decidio usar la misma interfaz HID
donde se puede hacer una comunicacion bilateral, enviando asi las conversiones obtenias por los 
potenciometros y obteniendo datos para procesarlos trabajar con los mismos. Para esto usamos la
libreria hidapi que permite una comunicacion sencilla con dispositivos HID en C. Esta libreria 
es multiplataforma y soporta Windows,Linux y MacOS. Cabe destacar que para que el dispositivo
Custom HID pueda obtener paquetes de datos, debemos definir algunas configuracines en el firmware
del microcontrolador. Primero, debemos setear flags para que el Ide nos permita hacer uso de las callbacks
de salida (OUT) en el modulo USB HID. Una de las flags mas importantes es `USBD_HID_OUTREPORT_ENABLED` que
habilita la recepcion de reportes de salida desde el host. Esta se setea como simbolo en las propiedasdes
del proyecto en CubeIde o como un define en el main.Luego, debemos definir un reporte de salida en el descriptor HID que
permita recibir datos desde el host. Tambien, debemos implementar la función de callback
`USBD_HID_OutEvent()` que se llama cuando el host envía un reporte de salida al dispositivo. 
Esta función recibe el buffer de datos enviados por el host. Pudiendo procesar estos paquetes para
utilizar un LCD_Groove para mostrar la información recibida. En este proyecto, se decidió enviar
datos de telemetría desde un simulador de carreras (Live For Speed) al microcontrolador. Se envia
un paquete de datos que contiene información como la velocidad del vehículo, RPM y marcha actual.
Una vez que se recibe este paquete de datos en la función de callback `USBD_HID_OutEvent()`, se utiliza
la libreria LCD_Groove para mostrar la información en el LCD utilizando la entrada i2c del microcontrolador.
El programa que arma y envia los paquetes utiliza la libreria hidapi para conectarse al dispositivo 
Custom HID. Este paquete de datos se estructura de acuerdo al descriptor HID definido en el firmware 
del microcontrolador. Se toman los datos de telemetría obtenidos del simulador y se empaquetan en el formato
definido. Luego, se utiliza la función `hid_write()` para enviar el paquete de datos al dispositivo Custom HID.
Esta funcion envia siempre un byte extra al inicio del paquete que indica el ID del reporte, por lo que
se debe tener en cuenta esto al armar el paquete de datos. 

El proposito de este proyecto es desarrollar un sistema embebido que pueda simular un dispositivo 
de entrada HID (volante, acelerador y freno) y al mismo tiempo recibir datos de telemetría desde un programa
el cual se muestra en un LCD. Utilizando la interfas USB HID y el reporte diseñado para asi lograr
comunicación bilateral entre el microcontrolador y la PC. Pudiendo desarrollar aplicaciones y hacer uso 
de este sistema embebido en simuladores de carreras u otros programas que utilicen dispositivos HID.
Esto se puede extender agregando mas funcionalidades al firmware, para asi poder desarrollar un sistema 
mas completo y con mayor funcionalidad. Siendo este un proyecto base para futuros desarrollos, ya que,
sorprendio la facilidad con la que se pudo implementar la comunicacion bilateral utilizando USB HID y
la respuesta obtenida.


## Definiciones de los reportes HID

** Reporte de entrada (IN) - Desde el microcontrolador a la PC

| Byte | Descripción            | Rango       |
|------|------------------------|-------------|
| 0    | Eje X (Volante)        | 0 - 255     |
| 1    | Eje Y (Acelerador)     | 0 - 255     |
| 2    | Eje Z (Freno)          | 0 - 255     |

Tamaños de los ejes: 8 bits/ 1 Byte cada uno
Tamaños del reporte IN: 3 Bytes (3 ejes * 1 Byte)


** Reporte de salida (OUT) - Desde la PC al microcontrolador

Tipo de paquete: 
 - 0: Paquete de inicializacion (6 Bytes RGB)

| Byte | Descripción            | Rango       |
|------|------------------------|-------------|
| 0    | ID del reporte         | 0x00        |
| 1    | Tipo de paquete        | 0 - 1       |
| 2    | Red                    | 0 - 65535   |
| 3    | Green                  | 0 - 65535   |
| 4    | Blue                   | 0 - 65535   |
| 5,6  | IGNORE                 |             |
 
 - 1: Paquete de telemetría (7 Bytes)
| Byte | Descripción            | Rango       |
|------|------------------------|-------------|
| 0    | ID del reporte         | 0x00        |
| 1    | Tipo de paquete        | 0 - 1       |
| 2,3  | RPM                    | 0 - 65535   |
| 4,5  | Velocidad (km/h)       | 0 - 65535   |
| 6    | Marcha actual          | 0 - 255     |
--- --- IGNORE ---

Tamaños del reporte OUT: 7 Bytes (1 Byte ID + 1 Byte Tipo de paquete
 + 2 Byte RPM + 2 Bytes Velocidad + 1 Byte Marcha)


 Aclaraciones:
 - El ID del reporte es un byte extra que se agrega al inicio del paquete
   al usar la funcion hid_write() de la libreria hidapi. Este ID es consumido
   por la API y envia los Bytes siguientes al microcontrolador.
- El tipo de paquete se utiliza para definir diferentes tipos de datos que se pueden enviar.
   En este caso, se utiliza 0 para enviar un paquete de inicializacion (usado en algunos 
   protocolos) donde se envian 6 Bytes en formato rgb (2 Bytes cada color), 
    y 1 para enviar el paquete de telemetría en el formato definido.
 - Los valores de RPM y Velocidad son enviados en formato Little Endian, es decir,
   el byte menos significativo primero.
 - La marcha actual se envia como un valor de 0 a 255, donde 255 representa Reversa, 0
   punto Neutro, 1 primera marcha, 2 segunda marcha, etc.
