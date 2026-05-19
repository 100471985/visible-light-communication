// Máster en Ingeniería de Sistemas Electrónicos y Aplicaciones
// Universidad Carlos III de Madrid
// Curso académico: 2025 - 26

// Proyectos Experimentales II
// Visual Light Communication - VLC

// Recepción

#include <stdint.h>

// Pines
#define PIN_INICIO PE_1   // Pin para detectar flanco de bajada
#define PIN_DATOS  PE_2   // Pin para leer el dato digital


#define TIEMPO_BIT           500                         // 2 kbps -> 500 us por bit
#define PRIMERA_MUESTRA      ((TIEMPO_BIT * 3) / 2)      // 1.5T -> centro del primer bit de datos
#define NUM_BITS_DATOS       8
#define TIEMPO_FIN_MENSAJE   3000

#define MAX_CARACTERES          100


volatile bool inicioDetectado = false;
volatile bool recepcionEnCurso = false;
volatile uint32_t tiempoInicio = 0;


uint8_t bitsTrama[NUM_BITS_DATOS];
uint8_t byteTrama = 0;
uint8_t bitParadaTrama = 1;
uint8_t errorTrama = 0;
bool transmisorApagado = false;


char mensaje[MAX_CARACTERES + 1];

uint8_t bitsMensaje[MAX_CARACTERES][NUM_BITS_DATOS];
uint8_t bitParadaMensaje[MAX_CARACTERES];
uint8_t errorMensaje[MAX_CARACTERES];

int longitudMensaje = 0;
bool mensajeActivo = false;
bool desbordamientoMensaje = false;
bool mensajeConError = false;

uint32_t tiempoUltimoCaracter = 0;


// Declaración de funciones
void ISRInicio(void);
void recibirTrama(uint32_t tiempoInicio);
void anadirCaracter(void);
void FinMensaje(void);
void imprimirMensaje(void);
void reiniciarMensaje(void);
void WaitUntil(uint32_t instanteObjetivoUs);


void setup()
{
  pinMode(PIN_INICIO, INPUT);
  pinMode(PIN_DATOS, INPUT);

  Serial.begin(9600);

  mensaje[0] = '\0';

  attachInterrupt(digitalPinToInterrupt(PIN_INICIO), ISRInicio, FALLING);

  Serial.println(">> Receptor VLC inicializado.");
  Serial.println(">> Formato: [Start 0] + [8 bits MSB primero] + [Stop 1]");
  Serial.println(">> Velocidad configurada: 2 kbps");
}


void loop()
{
  
  if (inicioDetectado && !recepcionEnCurso)
  {
    uint32_t tiempoInicioLocal;

    noInterrupts();
    tiempoInicioLocal = tiempoInicio;
    inicioDetectado = false;
    recepcionEnCurso = true;
    interrupts();

    // Leemos la trama completa: 8 bits + stop
    recibirTrama(tiempoInicioLocal);

    // Liberamos la recepción para poder detectar otro start
    noInterrupts();
    recepcionEnCurso = false;
    interrupts();

    // Añadimos el carácter recibido al mensaje
    anadirCaracter();
  }

  FinMensaje();
}


void ISRInicio(void)
{
   if (recepcionEnCurso || inicioDetectado)
  {
    return;
  }

  tiempoInicio = micros();
  inicioDetectado = true;
}


void recibirTrama(uint32_t tiempoInicio)
{
  byteTrama = 0;
  bitParadaTrama = 1;
  errorTrama = 0;
  transmisorApagado = false;

  for (int i = 0; i < NUM_BITS_DATOS; i++)
  {
    uint32_t instanteMuestreo = tiempoInicio + PRIMERA_MUESTRA + (i * TIEMPO_BIT);

    WaitUntil(instanteMuestreo);

    uint8_t bitLeido = digitalRead(PIN_DATOS) ? 1 : 0;

    bitsTrama[i] = bitLeido;

    byteTrama = (byteTrama << 1) | bitLeido;
  }

  uint32_t instanteBitParada = tiempoInicio + PRIMERA_MUESTRA + (NUM_BITS_DATOS * TIEMPO_BIT);

  WaitUntil(instanteBitParada);

  bitParadaTrama = digitalRead(PIN_DATOS) ? 1 : 0;

   bool datosTodoCero = true;

  for (int i = 0; i < NUM_BITS_DATOS; i++)
  {
    if (bitsTrama[i] != 0)
    {
      datosTodoCero = false;
    }
  }

  if (datosTodoCero && bitParadaTrama == 0)
  {
    transmisorApagado = true;
    errorTrama = 0;
  }
  else
  {
    if (bitParadaTrama != 1)
    {
      errorTrama = 1;
    }
  }
}


void anadirCaracter(void)
{

  if (transmisorApagado)
  {
    if (mensajeActivo && longitudMensaje > 0)
    {
      imprimirMensaje();
    }

    Serial.println("Transmisor apagado.");

    reiniciarMensaje();

    return;
  }

  if (longitudMensaje >= MAX_CARACTERES)
  {
    desbordamientoMensaje = true;
    return;
  }

  // Guardamos los bits de la trama
  for (int i = 0; i < NUM_BITS_DATOS; i++)
  {
    bitsMensaje[longitudMensaje][i] = bitsTrama[i];
  }

  // Guardamos stop bit y posible error
  bitParadaMensaje[longitudMensaje] = bitParadaTrama;
  errorMensaje[longitudMensaje] = errorTrama;

  // Convertimos el byte recibido a carácter ASCII
  if (errorTrama)
  {
    mensaje[longitudMensaje] = '?';
    mensajeConError = true;
  }
  else
  {
    if (byteTrama >= 32 && byteTrama <= 126)
    {
      mensaje[longitudMensaje] = (char)byteTrama;
    }
    else
    {
      mensaje[longitudMensaje] = '?';
    }
  }

  longitudMensaje++;
  mensaje[longitudMensaje] = '\0';

  mensajeActivo = true;
  tiempoUltimoCaracter = micros();
}


void FinMensaje(void)
{
  if (!mensajeActivo)
  {
    return;
  }

  // Si estamos recibiendo una trama, todavía no puede terminar el mensaje
  if (recepcionEnCurso || inicioDetectado)
  {
    return;
  }

  // Si la línea está a 0, puede estar empezando otra trama
  if (digitalRead(PIN_DATOS) == LOW)
  {
    return;
  }

  // Si la línea lleva un tiempo en 1, damos el mensaje por terminado
  if ((int32_t)(micros() - tiempoUltimoCaracter) >= TIEMPO_FIN_MENSAJE)
  {
    imprimirMensaje();
  }
}


void imprimirMensaje(void)
{
  Serial.print("Mensaje recibido: ");
  Serial.print(mensaje);

  Serial.print("   Tramas: ");

  for (int i = 0; i < longitudMensaje; i++)
  {
    Serial.print("[0 ");

    for (int j = 0; j < NUM_BITS_DATOS; j++)
    {
      Serial.print(bitsMensaje[i][j]);
    }

    Serial.print(" ");
    Serial.print(bitParadaMensaje[i]);

    if (errorMensaje[i])
    {
      Serial.print(" ERROR");
    }

    Serial.print("] ");
  }

  Serial.println();

  if (mensajeConError)
  {
    Serial.println(">> Aviso: el mensaje tuvo al menos un error de trama.");
  }

  if (desbordamientoMensaje)
  {
    Serial.println(">> Aviso: mensaje demasiado largo.");
  }

  reiniciarMensaje();
}


void reiniciarMensaje(void)
{
  longitudMensaje = 0;
  mensaje[0] = '\0';

  mensajeActivo = false;
  desbordamientoMensaje = false;
  mensajeConError = false;
}


void WaitUntil(uint32_t tiempoMuestra)
{
  while ((int32_t)(micros() - tiempoMuestra) < 0)
  {
    
  }
}
