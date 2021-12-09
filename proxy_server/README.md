# Proxy Server

## Estructura de la carpeta

* ***inc***: Se encuentran archivos .h con distintos defines de utilidad y funciones implementadas.

* ***src***: Se encuentran los archivo fuente del desafío.

* ***cloud_cfg.json***: Archivo de configuración para el servidor cloud.

* ***error_send.json***: Archivo de errores sobre paquetes.

* ***Makefile***: Makefile para compilar y generar el ejecutable.
  
* ***temperatura.json***: Archivo generado por el paquete sensors.

## Acceso rapido de la carpeta

* [Inc](/proxy_server/inc/)
  
* [Src](/proxy_server/src/)

* [Cloud CFG](/proxy_server/cloud_cfg.json)

* [Makefile](/proxy_server/Makefile)

* [Temperatura](/proxy_server/temperatura.json)
  
## Descripción de los Fuentes

* **archivo_funciones.c**: Contiene distintas funciones utilizadas para manejar archivos dentro del proyecto.

* **curl_funciones.c**: Contiene las funciones que hacen uso de la biblioteca de CURL.

* **http_funciones.c**: Contiene las funciones que son llamadas desde el código principal para hacer el POST HTTP. Las mismas se encargan de armar el paquete a enviar y controlar que la misma llego exitosamente.

* **main.c**: Contiene el código principal del programa, es decir el **proxy server**. El mismo se encarga de lanzar a su hijo y contiene la lógica de comunicación con el mismo.

* **temperatura.c**: Contiene el código principal del proceso **productor de información**. Se encarga de levantar la información de temperatura, parsear la misma y comunicarlo al **proxy server**.

* **Makefile**: Para correr
```sh
make 
```
* **Versiones**:
```sh
sensors version 3.6.0 with libsensors version 3.6.0

GNU Make 4.3
Este programa fue construido para x86_64-pc-linux-gnu

gcc version 10.3.0 (Ubuntu 10.3.0-1ubuntu1~20.10) 
```