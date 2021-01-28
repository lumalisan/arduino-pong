#include <Arduino.h>
#include <Ethernet2.h>
#include "EthRaw.h"

// PLACA 1
// Responsable de:
// - Leer el input del Joystick 1
// - Recoger datos de Joystick 2 de la Placa 2 por Eth.
// - Calcular la lógica de juego
// - Enviar los datos por serial a Processing

/******************************************************************************/
/** Variables generales *******************************************************/
/******************************************************************************/

// Estados de juego
#define MENU 0
#define PLAYING 1

// Tamaños en pixeles de la ventana de juego en Processing
#define MAX_SCREEN_X 1300
#define MAX_SCREEN_Y 840

// Inputs
#define PIN_Y A0           // Pin de input analógico
#define PIN_BUTT A5        // Pin de input del botón
#define PIN_BUTT_DIGITAL 2 // Pin de input del botón
#define PIN_BUZZ 12

#define START_SPEED 1 // Velocidad de la bola

#define PLAYER_HEIGHT 100 // Tamaño en píxeles del padel (vertical) de cada PJ
#define PLAYER_WIDTH 30   // Tamaño en píxeles del padel (horizontal) de cada PJ
#define PLAYER1_X 50
#define PLAYER2_X 1225

// Para añadir deadzone al joystick
#define DEADZONE 3
#define PLAYER_Y_DEFAULT 370

// Booleano que señaliza que hay que avanzar la lógica de juego
volatile bool timer_flag = false;

// Booleano para saber si el juego ya ha comenzado
volatile bool playing = false;

// Booleano para reproducir sonido de jugador 1
volatile bool playSound = false;
volatile uint8_t high_low = 0;

// Coordenadas de los jugadores (por defecto al centro de la ventana)
volatile uint16_t player1Y = PLAYER_Y_DEFAULT; // Igual a posInicialY1 en Processing
volatile uint16_t player2Y = PLAYER_Y_DEFAULT; // Igual a posInicialY2 en Processing

volatile uint8_t button1 = 255; // Estado del botón 1
volatile uint8_t button2 = 255; // Estado del botón 2

// Coordenadas y velocidades de la pelota
float ballX = MAX_SCREEN_X / 2;
float ballY = MAX_SCREEN_Y / 2;
float ballSpeedX = START_SPEED;
float ballSpeedY = 0;

uint8_t state = MENU; // Estado por defecto

uint16_t scoreP1 = 0; // Puntuación jugador 1
uint16_t scoreP2 = 0; // Puntuación jugador 2

// Constantes y variables para comunicación Ethernet
SOCKET sckt = 0;

const uint8_t my_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x11};

const uint8_t default_buf_len = ETH_MAC_LENGTH + ETH_MAC_LENGTH + ETH_ETYPE_LENGTH;

void print_eth_frame(uint8_t *frame, uint16_t frame_len);

// tx buffer
volatile uint8_t tx_buff[ETH_MAX_FRAME_SIZE];
volatile uint16_t tx_buff_len;

// rx buffer
uint8_t rx_buff[ETH_MAX_FRAME_SIZE];
uint16_t rx_buff_len;
uint16_t rx_etype;

uint16_t i;
char buff[30];

volatile bool debounce = true;

/******************************************************************************/
/** INTERRUPCIONES ************************************************************/
/******************************************************************************/

// ISR Timer 0 - Reproduce el sonido por el buzzer
ISR(TIMER0_COMPA_vect)
{
  digitalWrite(PIN_BUZZ, high_low);
  high_low = (high_low + 1) % 2;
}

// ISR Timer 1 - Según el valor de playSound, activa/desactiva el Timer 0
ISR(TIMER1_COMPA_vect)
{
  if (playSound == 1)
  {
    TCCR0B &= ~(1 << CS01); // Activa Timer 0
    playSound = false;
  }
  else
  {
    TCCR0B |= (1 << CS01); // Desactiva Timer 0
  }
}

// ISR Timer 3 - Reloj de juego
ISR(TIMER3_COMPA_vect)
{
  timer_flag = true;
}

// ISR Timer 4 - Polling Ethernet
ISR(TIMER4_COMPA_vect)
{
  if (w5500.getRXReceivedSize(sckt) != 0)
  {
    w5500.recv_data_processing(sckt, rx_buff, 2);
    w5500.execCmdSn(sckt, Sock_RECV);

    // Se descartan los primeros dos bytes que contienen la longitud
    rx_buff_len = rx_buff[0] << 8 | rx_buff[1];
    rx_buff_len -= 2;

    w5500.recv_data_processing(sckt, rx_buff, rx_buff_len);
    w5500.execCmdSn(sckt, Sock_RECV);

    player2Y = rx_buff[ETH_DATA_OFFSET + 0] << 8;
    player2Y |= rx_buff[ETH_DATA_OFFSET + 1];

    button2 = rx_buff[ETH_DATA_OFFSET + 2];
  }
}

/******************************************************************************/
/** MÉTODOS ADICIONALES *******************************************************/
/******************************************************************************/

// Actualiza la posición de la pelota
void updateBall()
{
  // Añade velocidad X e Y a la pelota
  ballX += ballSpeedX;
  ballY += ballSpeedY;

  // Aumentar puntuación y resetear la posicion de
  // la bola en funcion de que jugador ha puntuado
  if (ballX > MAX_SCREEN_X)
  {
    scoreP1++;
    ballX = MAX_SCREEN_X / 2;
    ballY = MAX_SCREEN_Y / 2;
    ballSpeedX = -START_SPEED;
    ballSpeedY = 0;
    return;
  }
  else if (ballX < 0)
  {
    scoreP2++;
    ballX = MAX_SCREEN_X / 2;
    ballY = MAX_SCREEN_Y / 2;
    ballSpeedX = START_SPEED;
    ballSpeedY = 0;
    return;
  }

  // Reflejo vertical
  if (ballY > MAX_SCREEN_Y || ballY < 0)
    ballSpeedY *= -1;

  // Si hay colisión con el padel del jugador 1 o del 2
  if (ballX == PLAYER1_X + PLAYER_WIDTH && ballY >= player1Y && ballY <= player1Y + PLAYER_HEIGHT)
  {
    // Si le da hado al paddle del jugador 1
    float paddleCenter = player1Y + (PLAYER_HEIGHT / 2);
    float d = paddleCenter - ballY;
    ballSpeedY += d * -0.1;
    ballSpeedX *= -1;
    playSound = true; // Reproduce el sonido
  }
  else if (ballX == PLAYER2_X && ballY >= player2Y && ballY <= player2Y + PLAYER_HEIGHT)
  {
    // Si le ha dado al paddle del jugador 2
    float paddleCenter = player2Y + (PLAYER_HEIGHT / 2);
    float d = paddleCenter - ballY;
    ballSpeedY += d * -0.1;
    ballSpeedX *= -1;
    playSoundPlaca2();
  }
}

// Envía una trama Ethernet a la placa 2 cuando hay una colisión entre
// pelota y Jugador 2, para que el buzzer de la placa haga sonido
void playSoundPlaca2()
{
  uint8_t temp = 1;
  tx_buff[ETH_DATA_OFFSET + 0] = temp;

  tx_buff_len = default_buf_len + sizeof(temp);

  w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
  w5500.execCmdSn(sckt, Sock_SEND_MAC);
}

/******************************************************************************/
/** SETUP *********************************************************************/
/******************************************************************************/

void setup()
{
  Serial.begin(115200);

  // Inicializamos los pines
  pinMode(PIN_Y, INPUT);
  pinMode(PIN_BUTT, INPUT_PULLUP);
  pinMode(PIN_BUTT_DIGITAL, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);

  // Inicialización W5500
  w5500.init();

  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);

  cli(); // Desactiva interrupciones

  // Timer 0 - Reproduce el sonido
  TCCR0A = 0;
  TCCR0B = 0; 
  TCNT0 = 0;
  OCR0A = 73;
  TCCR0B |= (1 << WGM01);
  TCCR0B |= (1 << CS02) | (0 << CS01) | (0 << CS00);
  TIMSK0 |= (1 << OCIE0A);

  // Timer 1 - Controla cuando reproducir el sonido
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 31249;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (0 << CS11) | (0 << CS10);
  TIMSK1 |= (1 << OCIE1A);

  // Timer 3 -  Reloj de juego
  TCCR3A = 0;
  TCCR3B = 0;
  TCCR3B |= (1 << WGM32);
  OCR3A = 10;
  TIMSK3 |= (1 << OCIE3A);
  TCCR3B |= (1 << CS30);
  TCCR3B |= (1 << CS32);

  // Timer 4 - Para el polling de Ethernet periódico
  TCCR4A = 0;
  TCCR4B = 0;
  TCCR4B |= (1 << WGM42);
  OCR4A = 25;
  TIMSK4 |= (1 << OCIE4A);
  TCCR4B |= (1 << CS40);
  TCCR4B |= (1 << CS42);

  sei(); // Rehabilita interrupciones

  // Prepare tx frame
  for (i = 0; i < ETH_MAC_LENGTH; i++)
    tx_buff[ETH_DST_OFFSET + i] = BCAST_MAC[i];
  for (i = 0; i < ETH_MAC_LENGTH; i++)
    tx_buff[ETH_SRC_OFFSET + i] = my_mac[i];

  tx_buff[ETH_ETYPE_OFFSET + 0] = (ETH_EXP_ETYPE & 0xFF00) >> 8;
  tx_buff[ETH_ETYPE_OFFSET + 1] = (ETH_EXP_ETYPE & 0x00FF);

  tx_buff[ETH_DATA_OFFSET] = 0;

  tx_buff_len = default_buf_len;
}

/******************************************************************************/
/** LOOP **********************************************************************/
/******************************************************************************/

void loop()
{
  // TABLA DE ESTADOS
  // 0 = Estamos en el menú
  // 1 = Estamos jugando

  while (1)
  {
    // Cuando se verifica una interrupción del Timer
    if (timer_flag)
    {
      switch (state)
      {
      case MENU:
        // Cargar la pantalla de menú y comprobar que los dos jugadores pulsan el joystick
        button1 = digitalRead(PIN_BUTT_DIGITAL);
        if (button1 == 0 && button2 == 0) state = PLAYING;
        break;
      case PLAYING:
        // Leemos el valor del joystick de la placa 1, lo mapeamos, le ponemos un
        // poco de deadzone y actualizamos el valor de la bola. Después enviamos 
        // todos los datos por serial a Processing.

        player1Y = analogRead(PIN_Y);

        // Hacemos el mapping de los valores del potenciometro a los
        // valores de nuestra ventana.
        player1Y = map(player1Y, 0, 1023, 0, 740);

        // Verificamos si los valores están fuera de nuestra
        // Deadzone para realmente mover el pad.
        if ((player1Y > PLAYER_Y_DEFAULT && player1Y < PLAYER_Y_DEFAULT + DEADZONE) ||
            (player1Y < PLAYER_Y_DEFAULT && player1Y > PLAYER_Y_DEFAULT - DEADZONE))
        {
          player1Y = PLAYER_Y_DEFAULT;
        }

        // Actualizamos la posición de la bola
        updateBall();

        // Enviamos los datos por serial a Processing
        Serial.print('<');
        Serial.print(player1Y);
        Serial.print(',');
        Serial.print(player2Y);
        Serial.print(',');
        Serial.print(state);
        Serial.print(',');
        Serial.print(scoreP1);
        Serial.print(',');
        Serial.print(scoreP2);
        Serial.print(',');
        Serial.print(ballX);
        Serial.print(',');
        Serial.print(ballY);
        Serial.println('>');
        break;

      default:
        break;
      }

      timer_flag = false;
    }
  }
}
