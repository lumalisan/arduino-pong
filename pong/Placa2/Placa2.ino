#include <Arduino.h>
#include <Ethernet2.h>
#include "EthRaw.h"

// PLACA 2
// Responsable de:
// - Leer el input del Joystick 2
// - Enviar los datos del Joystick 2 a la Placa 1

/******************************************************************************/
/** Variables generales *******************************************************/
/******************************************************************************/

#define PIN_Y A0           // Pin de input ANALógico
#define PIN_BUTT_DIGITAL 2 // Pin de input del botón
#define PIN_BUZZ 12        // Pin del buzzer

// Variables para introducir deadzone en el joystick
#define DEADZONE 3
#define PLAYER_Y_DEFAULT 370

// Coordenadas del jugador (por defecto al centro de la ventana)
uint16_t playerY = 405; // Igual a posInicialY2 en el file Processing

// Constantes y variables para comunicación Ethernet
SOCKET sckt = 0;

const uint8_t my_mac[] = {0x00, 0x00, 0x00, 0xFF, 0x32, 0x54};

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
volatile bool playSound = false; // Variable para reproducir el sonido
volatile uint8_t high_low = 0;

/******************************************************************************/
/** INTERRUPCIONES ************************************************************/
/******************************************************************************/

// Timers para el buzzer
ISR(TIMER0_COMPA_vect)
{
  digitalWrite(PIN_BUZZ, high_low);
  high_low = (high_low + 1) % 2;
}

ISR(TIMER1_COMPA_vect)
{
  if (playSound == 1)
  {
    TCCR0B &= ~(1 << CS01); // Activa Timer 0
    playSound = false;
    Serial.println("Playing...");
  }
  else
  {
    TCCR0B |= (1 << CS01); // Desactiva Timer 0
  }
}

// Rutina de interrupción Timer 3 (Envío Ethernet)
ISR(TIMER3_COMPA_vect)
{
  // DATOS A ENVIAR: Posición Y del analógico y estado del botón
  playerY = analogRead(PIN_Y);
  uint8_t butt = digitalRead(PIN_BUTT_DIGITAL);

  // Mapeamos los valores en estado 'raw'
  playerY = map(playerY, 0, 1023, 0, 740);

  // Deadzone player 2
  if ((playerY > PLAYER_Y_DEFAULT && playerY < PLAYER_Y_DEFAULT + DEADZONE) ||
      (playerY < PLAYER_Y_DEFAULT && playerY > PLAYER_Y_DEFAULT - DEADZONE))
  {
    playerY = PLAYER_Y_DEFAULT;
  }

  // Poner los datos en la trama ethernet en trozos de 1 byte
  tx_buff[ETH_DATA_OFFSET + 0] = (playerY & 0xFF00) >> 8;
  tx_buff[ETH_DATA_OFFSET + 1] = (playerY & 0x00FF);
  tx_buff[ETH_DATA_OFFSET + 2] = butt;
  tx_buff_len = default_buf_len + sizeof(playerY) + sizeof(butt);

  w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
  w5500.execCmdSn(sckt, Sock_SEND_MAC);
}

// Rutina de interrupción Timer 4 (Polling Ethernet)
ISR(TIMER4_COMPA_vect)
{
  // Si se ha recibido algo
  if (w5500.getRXReceivedSize(sckt) != 0)
  {
    w5500.recv_data_processing(sckt, rx_buff, 2);
    w5500.execCmdSn(sckt, Sock_RECV);

    // Se descartan los primeros dos bytes que contienen la longitud
    rx_buff_len = rx_buff[0] << 8 | rx_buff[1];
    rx_buff_len -= 2;

    w5500.recv_data_processing(sckt, rx_buff, rx_buff_len);
    w5500.execCmdSn(sckt, Sock_RECV);

    // Si la trama no es la que toca, la descartamos
    if (rx_buff[ETH_DATA_OFFSET] == 1) {
      playSound = true; // Reproduce el sonido
    }
  }
}

/******************************************************************************/
/** SETUP *********************************************************************/
/******************************************************************************/

void setup()
{
  Serial.begin(115200);

  // Inicializando los pines
  pinMode(PIN_Y, INPUT);
  pinMode(PIN_BUTT_DIGITAL, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);

  // Inicialización W5500
  w5500.init();

  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);

  cli();

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

  // Timer 3 - Para el envío de Ethernet periódico
  TCCR3A = 0;
  TCCR3B = 0;
  TCCR3B |= (1 << WGM32);
  OCR3A = 50;
  TIMSK3 |= (1 << OCIE3A);
  TCCR3B |= (1 << CS30);
  TCCR3B |= (1 << CS32);

  // Timer 4 - Para el polling de Ethernet periódico
  TCCR4A = 0;
  TCCR4B = 0;
  TCCR4B |= (1 << WGM42); 
  OCR4A = 100;
  TIMSK4 |= (1 << OCIE4A);
  TCCR4B |= (1 << CS40);
  TCCR4B |= (1 << CS42);

  sei();

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

// La placa 2 no hace nada en el loop
void loop() {}
