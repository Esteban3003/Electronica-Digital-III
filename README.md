# Grabador / Reproductor de Voz – LPC1769

Proyecto basado en la LPC1769 que permite grabar audio mediante ADC, reproducirlo por DAC utilizando DMA y transmitir las muestras por UART. El sistema opera mediante dos botones y un potenciómetro que ajusta la velocidad de reproducción.

---

## Funcionalidad General

### Grabación de audio
- La grabación se inicia mediante el botón **EINT0**.
- Se capturan muestras a **8 kHz** utilizando el ADC.
- Se almacenan **12000 muestras** por sesión (aprox. 1.5 s).
- Al completar la captura, el sistema:
  - Detiene el proceso de grabación.
  - Procesa las muestras para el DAC.
  - Habilita el envío de datos por UART.

---

### Envío por UART
- Luego de finalizar la grabación, las muestras se transmiten automáticamente por **UART2**.
- Formato de dato enviado: **binario**, 2 bytes por muestra.
- Útil para almacenamiento en PC, análisis o graficación externa.

---

### Reproducción de audio
- La reproducción se controla mediante el botón **EINT1**.
- Al activarse, se habilita el DAC y el canal DMA para reproducir las muestras cargadas.
- La reproducción es continua, empleando una lista de descriptores LLI para mantenimiento del flujo.
- Presionar nuevamente **EINT1** detiene la reproducción y apaga el DMA.

---

### Control de velocidad de reproducción
- Un potenciómetro conectado al ADC permite modificar el **timeout del DAC**, afectando la velocidad de salida.
- El ajuste se realiza en tiempo real y está activo únicamente cuando no se está grabando.

---

## Interfaz Física

### Botones
| Botón | Función |
|-------|---------|
| **EINT0** | Inicia una nueva sesión de grabación |
| **EINT1** | Inicia o detiene la reproducción |

### Señalización por LEDs
| LED | Estado representado |
|-----|---------------------|
| Azul | Grabación en curso |
| Verde | Reproducción activa |
| Rojo | Estado inactivo |

---

## Flujo operativo

1. Presionar **EINT0** → Se borran las muestras previas y comienza una nueva grabación.  
2. Al completar las 12000 muestras → Se procesan y se envían por UART.  
3. Presionar **EINT1** → Se reproducen las muestras mediante DAC + DMA.  
4. Presionar nuevamente **EINT1** → Se detiene la reproducción.

---

## Características técnicas

- **ADC:** modo burst, dos canales (audio y potenciómetro).  
- **DAC:** habilitado para DMA con ajuste dinámico de timeout.  
- **DMA:** configurado en modo memoria-a-periférico mediante listas enlazadas.  
- **UART:** transmisión en formato binario para exportación de muestras.  
- **Tamaño de buffer:** `LISTSIZE = 12000` muestras de 10 bits.  

---

