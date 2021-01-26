#include <Arduino.h>
#include <Ethernet2.h>
#include "EthRaw.h"


// PLACA 2
// Responsable de:
// - Leer el input del Joystick 2
// - Enviar los datos del Joystick 2 a la Placa 1

/**************************************/
/* Variables generales                */
/**************************************/

#define PIN_Y A0    // Pin de input ANALógico
#define PIN_BUTT A5  // Pin de input del botón

// Coordenadas del jugador (por defecto al centro de la ventana)
uint16_t playerY = 405;  // Igual a posInicialY2 en el file Processing

// Constantes y variables para comunicación Ethernet
SOCKET sckt = 0;

const uint8_t my_mac[] = {0x00, 0x00, 0x00, 0xFF, 0x32, 0x54};

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

// Tarea periódica de actualización de juego y envío de datos
ISR(TIMER1_COMPA_vect)
{
  //timer_flag = true;
}

/******************************************************************************/
/** SETUP *********************************************************************/
/******************************************************************************/

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_Y, INPUT);
  pinMode(PIN_BUTT, INPUT);

  /*
  // Configure timer 1 (timer de juego)
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR1A = 0;
  TCCR1B = 0;

  // HAY Q SINCRONZAR O NO¿?¿¿?¿¿??¿?¿?¿?¿?¿?¿?¿?¿?¿???¿?¿?¿?¿?¿?¿?¿
  TCCR1B |= (1 << WGM12); // CTC => WGMn3:0 = 0100
  OCR1A = 1000;
  TIMSK1 |= (1 << OCIE1A);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  */

  // Inicialización W5500
  w5500.init();

  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);

  // Configure timer 3 para envío Ethernet periódico
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR3A = 0;
  TCCR3B = 0;

  TCCR3B |= (1 << WGM32); // CTC => WGMn3:0 = 0100
  OCR3A = 25;
  TIMSK3 |= (1 << OCIE3A);
  TCCR3B |= (1 << CS30);
  TCCR3B |= (1 << CS32);

  /*
  // Configure timer 4 para polling Ethernet periódico (Posiblemente no necesario)
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR4A = 0;
  TCCR4B = 0;

  TCCR4B |= (1 << WGM42); // CTC => WGMn3:0 = 0100
  OCR4A = 3000;
  TIMSK4 |= (1 << OCIE4A);
  TCCR4B |= (1 << CS40);
  TCCR4B |= (1 << CS42);
  */

  // Prepare tx frame
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_DST_OFFSET + i] = BCAST_MAC[i];
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_SRC_OFFSET + i] = my_mac[i];

  tx_buff[ETH_ETYPE_OFFSET + 0] = (ETH_EXP_ETYPE & 0xFF00) >> 8;
  tx_buff[ETH_ETYPE_OFFSET + 1] = (ETH_EXP_ETYPE & 0x00FF);

  tx_buff[ETH_DATA_OFFSET] = 0;

  tx_buff_len = default_buf_len;

}

// Rutina de interrupción Timer 3 (Envío Ethernet)
ISR(TIMER3_COMPA_vect)
{
  // Enviar datos
  Serial.println(" Send datos ethernet");

  // DATOS A ENVIAR: Posición Y del ANALógico y estado del botón

  playerY = analogRead(PIN_Y);
  uint8_t butt = analogRead(PIN_BUTT);
  
  Serial.print("Valor de analogíco Y: ");
  Serial.println(playerY);
  if (butt == 0) {
    Serial.println("BOTÓN PULSADO HOSTIA");
  }
  //Serial.print("Valor del botón: ");
  //Serial.println(butt);

  // Poner los datos en la trama ethernet en trozos de 1 byte
  tx_buff[ETH_DATA_OFFSET + 0] = (playerY & 0xFF00) >> 8;
  tx_buff[ETH_DATA_OFFSET + 1] = (playerY & 0x00FF);
  tx_buff[ETH_DATA_OFFSET + 2] = butt;
  tx_buff_len = default_buf_len + sizeof(playerY) + sizeof(butt);

  //tx_buff[ETH_DATA_OFFSET] = pot;
  w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
  w5500.execCmdSn(sckt, Sock_SEND_MAC);
}

/*
// Rutina de interrupción Timer 4 (Polling Ethernet)
ISR(TIMER4_COMPA_vect)
{
  Serial.println("No he entrado!!!");
  if (w5500.getRXReceivedSize(sckt) != 0)
    {
      Serial.println("Received ");

      w5500.recv_data_processing(sckt, rx_buff, 2);
      w5500.execCmdSn(sckt, Sock_RECV);

      // Se descartan los primeros dos bytes que contienen la longitud
      rx_buff_len = rx_buff[0] << 8 | rx_buff[1];
      rx_buff_len -= 2;

      sprintf(buff, "(%u bytes)\n\r", rx_buff_len);
      Serial.print(buff);

      w5500.recv_data_processing(sckt, rx_buff, rx_buff_len);
      w5500.execCmdSn(sckt, Sock_RECV);

      // Print etype
      rx_etype = (rx_buff[ETH_ETYPE_OFFSET] << 8) | rx_buff[ETH_ETYPE_OFFSET + 1];
      sprintf(buff, "   ETYPE: 0x%x", rx_etype);
      Serial.println(buff);

      if (rx_etype == ETH_EXP_ETYPE)
      {
        print_eth_frame(rx_buff, rx_buff_len);
      }
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
*/

/******************************************************************************/
/** LOOP **********************************************************************/
/******************************************************************************/

void loop()
{
  /*
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
