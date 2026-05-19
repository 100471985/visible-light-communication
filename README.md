# Sistema de Comunicación VLC

Este repositorio contiene el código fuente para el diseño e implementación de un sistema de comunicación por luz visible (VLC) con interfaz IoT.

## Estructura del código:
* **/Raspberry_Pi**: Contiene el servidor web Flask (Python) y el Front-End (HTML/JS/CSS).
* **/TivaC_TX**: Firmware en C del microcontrolador emisor (Modulación BFSK y control PWM).
* **/TivaC_RX**: Firmware en C del microcontrolador receptor (Máquina de estados para la lectura asincrona de los datos, UART like).

## Hardware principal:
* Raspberry Pi 3B+
* Texas Instruments Tiva C Series (TM4C123GH6PM)
* PLL CD4046 y Front-End analógico a medida.
