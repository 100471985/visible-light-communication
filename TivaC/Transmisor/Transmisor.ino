// Máster en Ingeniería de Sistemas Electrónicos y Aplicaciones
// Universidad Carlos III de Madrid
// Curso académcio: 2025 - 26

// Proyectos Experimentales II
// Visual Light Communication - VLC

// Autores:
// Manuel Pomares
// José Ortiz


// Bibliotecas
#include <stdint.h>              // standard library for integers 
#include <stdlib.h>              // standard library for atoi() (text -> number)
#include "driverlib/systick.h"   // standard library for the SysTick (header)
#include "driverlib/systick.c"   // standard library for the SysTick (functions)
#include "wiring_private.h"      // library to access the PWM with configurable frequency


// Hardware
#define ledPin PN_1 // LED de estado
#define PWM PM_0    // PWM BFSK


// Parámetros sistema
// PERIOD OF THE SYSTICK COUNTER. THE CLOCK FREQUENCY IS 120 MHz.
#define TickerPeriod 1200  // @ 100 kHz
#define fhigh 190000      // Frecuencia alta -> 1 lógico
#define flow  110000      // Frecuencia baja -> 0 lógico


// Variables sistema
volatile bool flagTicker = false;
volatile bool isSystemOn = false;
volatile int currentDuty = 50;
volatile bool isTransmitting = false;
volatile bool bitBuffer[1000]; // Caben hasta 100 caracteres (1 caracter = 10 bits)
volatile int totalBits = 0;
volatile int bitActual = 0;


// Prototipos de funciones
void setSystemState(bool state);
void updateDimming(int value);
void prepararMensaje(String& msg);
inline void transmitirBit();


void setup()
{
  // 1. Configurar Ticker y Systick
  SysTickDisable();                      // Disables SysTick during the configuration
  SysTickPeriodSet(TickerPeriod);        // Define the period of the counter. When it reaches 0, it activates the interrupt
  SysTickIntRegister(&Ticker);           // The interrupt is associated to the SysTick ISR
  SysTickIntEnable();                    // The SysTick interrupt is enabled
  
  // 2. Configuramos el LED de estado
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // 3. Configuramos PWM
  pinMode(PWM, OUTPUT);

  // 4. Inicializamos el Monitor Serie (PC)
  Serial.begin(9600);

  // 5. Inicializamos el UART7 (Raspberry Pi)
  Serial7.begin(9600);

  // 6. Habilitar Ticker e Interrupciones
  SysTickEnable();                       // The SysTick is enabled, after having been configured
  IntMasterEnable();                     // All interrupts are enabled

  // 7. Sistema inicializado
  Serial.println(">> Sistema VLC inicializado.");
}


void loop()
{
  if (Serial7.available() > 0)
  {
    char comando = Serial7.read();

    // ==========================================
    // 1. COMANDO SISTEMA (S0 o S1)
    // ==========================================
    if (comando == 'S')
    {
      while (!Serial7.available());
      char estado = Serial7.read();  // Leemos SIEMPRE para limpiar la UART

      if (!isTransmitting)
      {
        setSystemState(estado == '1');
      }

      else
      {
        Serial.println(">> SISTEMA TRANSMITIENDO. No se puede apagar.");
      }
    }

    // ==========================================
    // 2. COMANDO DUTY CYCLE (D + numero + \n)
    // ==========================================
    else if (comando == 'D')
    {
      char buffer[10];
      int index = 0;

      while (true)
      {
        if (Serial7.available())
        {
          char c = Serial7.read();
          if (c == '\n') break;
          if (index < 9) buffer[index++] = c;
        }
      }
      buffer[index] = '\0';

      if (!isTransmitting) 
      {
        updateDimming(atoi(buffer));
      }

      else
      {
        Serial.println(">> SISTEMA TRANSMITIENDO: No se puede hacer dimming.");
      }
    }

    // ==========================================
    // 3. COMANDO TRANSMISION BFSK
    // ==========================================
    else if (comando == 'T')
    {
      if (isSystemOn && !isTransmitting)
      {
        String mensaje = Serial7.readStringUntil('\n');
        prepararMensaje(mensaje);
      }

      else
      {
        Serial.println(">> SISTEMA APAGADO o TRANSMITIENDO.");
      }
    }
  }
}


void Ticker(void)
{
  static uint16_t cont = 0;

  // Divisor de frecuencia 100 kHz -> 2 kHz
  if (++cont >= 50)
  {
    cont = 0;
    transmitirBit();
  }
}

void setSystemState(bool state)
{
  isSystemOn = state; // Actualizamos la variable global

  if (isSystemOn)
  {
    PWMWrite(PWM, 100, currentDuty, fhigh);  // Encendemos PWM
    digitalWrite(ledPin, HIGH);             // LED de estado ON
    Serial.println(">>> Accion: SISTEMA ON");
  }

  else
  {
    PWMWrite(PWM, 100, 0, fhigh);            // Apagamos PWM
    digitalWrite(ledPin, LOW);              // LED de estado OFF
    Serial.println(">>> Accion: SISTEMA OFF");
  }
}

void updateDimming(int value)
{
  currentDuty = value; // Guardamos siempre el valor en la variable global

  // Solo aplicamos el cambio al hardware si el sistema está encendido
  if (isSystemOn)
  {
    PWMWrite(PWM, 100, currentDuty, fhigh);
    Serial.print(">>> PWM actualizado: ");
  }

  Serial.print(currentDuty);
  Serial.println("%");
}

void prepararMensaje(String& msg)
{
  int index = 0;

  // Cada letra ocupa 10 bits (1 start + 8 data + 1 stop)
  if (msg.length() > 100)
  {
    msg = msg.substring(0, 100);
    Serial.println(" >> Aviso: Mensaje truncado a 100 caracteres)");
  }

  // Convertimos ASCII a Bits (Empaquetado UART)
  for (int i = 0; i < msg.length(); i++)
  {
    // 1. Bit de INICIO para esta letra (Frecuencia baja)
    bitBuffer[index++] = 0;

    // 2. Los 8 bits de datos
    char c = msg[i];
    for (int b = 7; b >= 0; b--)
    {
      bitBuffer[index++] = bitRead(c, b);
    }

    // 3. Bit de PARADA para esta letra (Frecuencia alta / Reposo)
    bitBuffer[index++] = 1;
  }

  totalBits = index;
  bitActual = 0;


  // ==================================================
  // DEPURACIÓN UART (10 bits por letra)
  // ==================================================
  Serial.print(">>> Trama a enviar (");
  Serial.print(totalBits);
  Serial.print(" bits): ");

  for (int i = 0; i < totalBits; i++)
  {
    int posEnLetra = i % 10;

    if (posEnLetra == 0) Serial.print("["); // Inicio de letra

    Serial.print(bitBuffer[i]);

    if (posEnLetra == 0) Serial.print(" ");      // Espacio tras bit Start
    if (posEnLetra == 8) Serial.print(" ");      // Espacio antes de bit Stop
    if (posEnLetra == 9) Serial.print("]  ");    // Fin de letra
  }
  Serial.println();

  isTransmitting = true;
}

inline void transmitirBit()
{
  if (isSystemOn && isTransmitting)
  {
    if (bitActual < totalBits)
    {
      bool bit = bitBuffer[bitActual];

      if (bit == 1)
      {
        PWMWrite(PWM, 100, currentDuty, fhigh); // Frecuencia alta para el 1
      }

      else
      {
        PWMWrite(PWM, 100, currentDuty, flow);  // Frecuencia baja para el 0
      }

      bitActual++;
    }

    else
    {
      // Hemos terminado el mensaje
      isTransmitting = false;
      PWMWrite(PWM, 100, currentDuty, fhigh); // Volvemos al estado de reposo (1)
    }
  }
}
