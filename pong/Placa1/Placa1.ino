#include <Arduino.h>
#include <Ethernet2.h>
#include "EthRaw.h"


// PLACA 1
// Responsable de:
// - Leer el input del Joystick 1
// - Recoger datos de Joystick 2 de la Placa 2 por Eth.
// - Calcular la lógica de juego
// - Enviar los datos por serial a Processing

/**************************************/
/* Variables generales                */
/**************************************/

#define UP 0
#define DOWN 1
#define MENU 2
#define GOVER 3

// Tamaños en pixeles de la ventana de juego en Processing
#define MAX_SCREEN_X 1300
#define MAX_SCREEN_Y 840

#define PIN_Y A0    // Pin de input ANALógico
#define PIN_BUTT A5 // Pin de input del botón

// Booleano que señaliza que hay que avanzar la lógica de juego
volatile bool timer_flag = false;

// Coordenadas de los jugadores (por defecto al centro de la ventana)
uint16_t player1Y = 405; // Igual a posInicialY1 en el file Processing
uint16_t player2Y = 405; // Igual a posInicialY2 en el file Processing

uint8_t button1 = 0; // Estado del botón 1
uint8_t button2 = 0; // Estado del botón 2

// Coordenadas de la pelota
uint16_t ballX = MAX_SCREEN_X / 2;
uint16_t ballY = MAX_SCREEN_Y / 2;

uint8_t state = 255;  // Estado por defecto

uint16_t scoreP1 = 0;   // Puntuación jugador 1
uint16_t scoreP2 = 0;   // Puntuación jugador 2

// Constantes y variables para comunicación Ethernet

SOCKET sckt = 0;

const uint8_t my_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x11};

const uint8_t default_buf_len = ETH_MAC_LENGTH + ETH_MAC_LENGTH + ETH_ETYPE_LENGTH;

void print_eth_frame(uint8_t *frame, uint16_t frame_len);

// tx buffer
volatile uint8_t  tx_buff[ETH_MAX_FRAME_SIZE];
volatile uint16_t tx_buff_len;

// rx buffer
uint8_t  rx_buff[ETH_MAX_FRAME_SIZE];
uint16_t rx_buff_len;
uint16_t rx_etype;

uint16_t i;
char buff[30];

volatile bool debounce = true;


ISR(TIMER1_COMPA_vect)
{
  timer_flag = true;
}

// Actualiza la posición de la pelota
void updateBall() {

}

// Reset del juego: Restablecimiento de parámetros por defecto
void reset() {
  timer_flag = false;

  // Coordenadas jugadores
  player1Y = 405; // Igual a posInicialY1 en el file Processing
  player2Y = 405; // Igual a posInicialY2 en el file Processing

  // Coordenadas de la pelota
  ballX = MAX_SCREEN_X / 2;
  ballY = MAX_SCREEN_X / 2;

  state = 255;

  // Puntuaciones
  scoreP1 = 0;
  scoreP2 = 0;

  TCCR1B &= ~(1 << CS11); // Habilita Timer
  OCR1A = 1000;
}


/******************************************************************************/
/** SETUP *********************************************************************/
/******************************************************************************/

void setup()
{
  Serial.begin(115200);

  // Configure timer 1
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << WGM12); // CTC => WGMn3:0 = 0100
  OCR1A = 1000;
  TIMSK1 |= (1 << OCIE1A);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);

  // Inicialización W5500
  w5500.init();

  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);

  /*
    // Configure timer 3 para envío Ethernet periódico
    // 10 ms; OC = 10; pre-escaler = 1:1024
    TCCR3A = 0;
    TCCR3B = 0;

    TCCR3B |= (1 << WGM32); // CTC => WGMn3:0 = 0100
    OCR3A = 500;
    TIMSK3 |= (1 << OCIE3A);
    TCCR3B |= (1 << CS30);
    TCCR3B |= (1 << CS32);
  */

  // Configure timer 4 para polling Ethernet periódico
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR4A = 0;
  TCCR4B = 0;

  TCCR4B |= (1 << WGM42); // CTC => WGMn3:0 = 0100
  OCR4A = 500;
  TIMSK4 |= (1 << OCIE4A);
  TCCR4B |= (1 << CS40);
  TCCR4B |= (1 << CS42);

  // Prepare tx frame
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_DST_OFFSET + i] = BCAST_MAC[i];
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_SRC_OFFSET + i] = my_mac[i];

  tx_buff[ETH_ETYPE_OFFSET + 0] = (ETH_EXP_ETYPE & 0xFF00) >> 8;
  tx_buff[ETH_ETYPE_OFFSET + 1] = (ETH_EXP_ETYPE & 0x00FF);

  tx_buff[ETH_DATA_OFFSET] = 0;

  tx_buff_len = default_buf_len;
}

/*
  // Rutina de interrupción Timer 3 (Envío Ethernet)
  ISR(TIMER3_COMPA_vect)
  {
  // Enviar datos
  Serial.println(" Send datos ethernet");

  uint16_t temp = 69;  // Variable temporal para poner algo

  // Poner los datos en la trama ethernet en trozos de 1 byte
  tx_buff[ETH_DATA_OFFSET + 0] = (temp & 0xFF00) >> 8;
  tx_buff[ETH_DATA_OFFSET + 1] = (temp & 0x00FF);
  tx_buff_len = default_buf_len + sizeof(temp);

  //tx_buff[ETH_DATA_OFFSET] = pot;
  w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
  w5500.execCmdSn(sckt, Sock_SEND_MAC);

  }
*/

// Rutina de interrupción Timer 4 (Polling Ethernet)
ISR(TIMER4_COMPA_vect)
{
  if (w5500.getRXReceivedSize(sckt) != 0)
  {
    Serial.print("Receive ");

    w5500.recv_data_processing(sckt, rx_buff, 2);
    w5500.execCmdSn(sckt, Sock_RECV);

    // Se descartan los primeros dos bytes que contienen la longitud
    rx_buff_len = rx_buff[0] << 8 | rx_buff[1];
    rx_buff_len -= 2;

    sprintf(buff, "(%u bytes)\n\r", rx_buff_len);
    Serial.print(buff);

    w5500.recv_data_processing(sckt, rx_buff, rx_buff_len);
    w5500.execCmdSn(sckt, Sock_RECV);

    player2Y = rx_buff[ETH_DATA_OFFSET + 0] << 8;
    player2Y |= rx_buff[ETH_DATA_OFFSET + 1];

    button2 = rx_buff[ETH_DATA_OFFSET + 2];

    Serial.print("Player 2Y vale: ");
    Serial.println(player2Y);

    Serial.print("El botón 2 vale: ");
    Serial.println(button2);
    

    /*
          // Print etype
          rx_etype = (rx_buff[ETH_ETYPE_OFFSET] << 8) | rx_buff[ETH_ETYPE_OFFSET + 1];
          sprintf(buff, "   ETYPE: 0x%x", rx_etype);
          Serial.println(buff);

          if (rx_etype == ETH_EXP_ETYPE)
          {
            print_eth_frame(rx_buff, rx_buff_len);
          }
        }
    */
  }
}


// Imprime datos recibidos por ethernet
void print_eth_frame(uint8_t *frame, uint16_t frame_len)
{
  char buff[30];
  uint16_t i;

  // Print dst
  sprintf(
    buff, "   DST: %02x:%02x:%02x:%02x:%02x:%02x",
    frame[ETH_DST_OFFSET + 0], frame[ETH_DST_OFFSET + 1], frame[ETH_DST_OFFSET + 2],
    frame[ETH_DST_OFFSET + 3], frame[ETH_DST_OFFSET + 4], frame[ETH_DST_OFFSET + 5]
  );
  Serial.println(buff);

  // Print src
  sprintf(
    buff, "   SRC: %02x:%02x:%02x:%02x:%02x:%02x",
    frame[ETH_SRC_OFFSET + 0], frame[ETH_SRC_OFFSET + 1], frame[ETH_SRC_OFFSET + 2],
    frame[ETH_SRC_OFFSET + 3], frame[ETH_SRC_OFFSET + 4], frame[ETH_SRC_OFFSET + 5]
  );
  Serial.println(buff);

  // Print data
  Serial.print("   DATA: ");
  for (i = 0; i < frame_len - 14; i++)
  {
    sprintf(buff, "%02x", frame[ETH_DATA_OFFSET + i]);
    Serial.print(buff);

    if      ((i + 1) % 16 == 0)      Serial.print("\n         ");
    else if ((i + 1) % 8  == 0)      Serial.print(" ");
    else if (i < frame_len - 14 - 1) Serial.print(":");
  }

  Serial.println("");
}


/******************************************************************************/
/** LOOP **********************************************************************/
/******************************************************************************/

void loop()
{
  /*
  // TABLA DE ESTADOS
  // 0 = Hacia la derecha
  // 1 = Hacia abajo
  // 2 = Hacia la izquierda
  // 3 = Hacia arriba
  // 4 = Game Over
  // 255 = Default (sin movimiento)

  // Tecla recibida
  uint8_t key;

  updateFood();

  while (1)
  {
    // Cuando se verifica una interrupción del Timer
    if (timer_flag)
    {
      // Según hacia dónde va la serpiente, cambia su posición
      switch (state)
      {
        case RIGHT: snakeX += 1; break;
        case DOWN:  snakeY += 1; break;
        case LEFT:  snakeX -= 1; break;
        case UP:    snakeY -= 1; break;
        case GOVER: TCCR1B |= (1 << CS11); break; // Pausa el timer
        default: break;
      }

      // Si la serpiente choca con los límites del campo, game over
      if (snakeX == MAX || snakeY == MAX || snakeX == 0 || snakeY == 0)
        state = 4;

      // Si la serpiente y la comida colisionan, suma 1 a la puntuación,
      // actualiza la posición de la comida y aumenta la velocidad del juego
      if (snakeX == foodX && snakeY == foodY) {
        score++;
        updateFood();

        OCR1A -= 400;
        if (OCR1A < 2000) OCR1A = 2000; // Límite máximo
      }

      Serial.print('<');
      Serial.print(snakeX);
      Serial.print(',');
      Serial.print(snakeY);
      Serial.print(',');
      Serial.print(foodX);
      Serial.print(',');
      Serial.print(foodY);
      Serial.print(',');
      Serial.print(state);
      Serial.print(',');
      Serial.print(score);
      Serial.println('>');

      timer_flag = false;
    }

    if (Serial.available() > 0)
    {
      key = Serial.read();

      // Control movement
      // UP 38, LEFT 37, RIGHT 39, DOWN 40

      switch (key) {
        // Ni idea de porqué A y S están intercambiados, pero si no no funciona
        case 'A': TCCR1B &= ~(1 << CS11); break; // Reanuda el timer
        case 'S': TCCR1B |= (1 << CS11); break;  // Pausa el timer
        case 'R': reset();

        case 38: state = UP; break;
        case 37: state = LEFT; break;
        case 39: state = RIGHT; break;
        case 40: state = DOWN; break;

        default: state = 255; break;
      }

    }
  }
  */
}
