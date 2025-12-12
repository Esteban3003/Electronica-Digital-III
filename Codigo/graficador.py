import serial
import numpy as np
import matplotlib.pyplot as plt

# CONFIGURAR SEGÚN TU PUERTO Y BAUDRATE
COM_PORT = 'COM5'    # Cambiá al puerto donde conectaste la LPC
BAUDRATE = 9600
LISTSIZE = 12000     # Debe coincidir con el tamaño que grabás en LPC

# Abrir puerto serie
ser = serial.Serial('COM5', BAUDRATE, timeout=20)

print("Esperando datos de la LPC...")

# Buffer para recibir los datos
data = []

for _ in range(LISTSIZE):
    # Leer 2 bytes por muestra (MSB, LSB)
    raw = ser.read(2)
    if len(raw) < 2:
        print("Error: no se recibieron suficientes datos")
        break
    # Reconstruir valor de 10 bits
    sample = ((raw[0] & 0x03) << 8) | raw[1]
    data.append(sample)

ser.close()
print("Datos recibidos. Graficando...")

# Convertir a array de numpy
data = np.array(data)

# Graficar
plt.figure(figsize=(12,4))
plt.plot(data)
plt.title("Señal grabada desde LPC")
plt.xlabel("Muestra")
plt.ylabel("Valor ADC")
plt.grid(True)
plt.show()