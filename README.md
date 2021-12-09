# Desafío Técnico 2021

## Acceso Rápido del Readme

  - [Introducción](#introducción)
  - [Solución Propuesta](#solución-propuesta)
  - [Estructura de la carpeta](#estructura-de-la-carpeta)
  - [Acceso rapido de la carpeta](#acceso-rapido-de-la-carpeta)
  - [Descripción de los Fuentes](#descripción-de-los-fuentes)

## Introducción

Se debe proponer una solución en base a un sistema embebido cuyo sistema operativo a utilizar es Linux. En el sistema hay varios procesos corriendo que se deben comunicar entre sí, e incluso consumir recursos de un sistema cloud.

Diagrama en bloques propuesto:

![alt text](https://github.com/nicoriostaurasi/desafio_tecnico_mirgor/blob/main/img/diagrama_en_bloques.png?raw=true "Logo Title Text 1")

El diagrama en bloques del sistema propuesto se presenta en la figura anterior. En base al mismo se pide, utilizando un lenguaje de programación con el que se sienta cómodo y que considere apropiado, y utilizando como plataforma de desarrollo una PC:

* **1.** Proponer un método de comunicación entre los distintos procesos productores de información y un proceso que actuará como consumidor, el cual debe encargarse al mismo tiempo de la comunicación con el servidor cloud (a este proceso se lo conoce como proceso Proxy). Describir qué protocolos utilizará y el formato de los mensajes entre los procesos.

* **2.** En base al método de comunicación y al formato de mensajes propuesto, implemente uno de los procesos productores cuya fuente de información sea la temperatura del CPU del sistema. El proceso deberá simplemente tomar una lectura de la temperatura del CPU del sistema cada un segundo y transmitirla al proceso consumidor (Proxy). 

* **3.** En base al sistema propuesto, ¿Cómo implementaría la comunicación entre el proceso consumidor (Proxy) y el servidor cloud? Proponga un diagrama de bloques y un diagrama de secuencia del funcionamiento del proceso. 

## Solución Propuesta

## Estructura de la carpeta

* ***inc***: Se encuentran archivos .h con distintos defines de utilidad y funciones implementadas

## Acceso rapido de la carpeta

* [Inc](/GuiaTP_02/servidor/inc/)
  
* [Src](/GuiaTP_02/servidor/src/)

* [CFG](/GuiaTP_02/servidor/cfg.txt)

* [Makefile](/GuiaTP_02/servidor/Makefile)


## Descripción de los Fuentes

* **driver_handler.c**: Contiene la función que ejecuta el proceso colector de datos [Fuente](src/driver_handler.c)
