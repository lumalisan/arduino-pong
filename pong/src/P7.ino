#include <Ethernet2.h>
#include "EthRaw.h"

#define PIN_POT   A0
#define PIN_BUTT  2

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


////////////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_BUTT, INPUT_PULLUP);
  pinMode(PIN_POT, INPUT);

  w5500.init();

  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);

  // Configure timer 1
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << WGM12); // CTC => WGMn3:0 = 0100
  OCR1A = 16400; // 500 ms / 64 us = 7812 ticks
  TIMSK1 |= (1 << OCIE1A);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);

  // Configure button pin interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_BUTT), pin_ISR, FALLING);


  // Prepare tx frame
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_DST_OFFSET + i] = BCAST_MAC[i];
  for (i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_SRC_OFFSET + i] = my_mac[i];

  tx_buff[ETH_ETYPE_OFFSET + 0] = (ETH_EXP_ETYPE & 0xFF00) >> 8;
  tx_buff[ETH_ETYPE_OFFSET + 1] = (ETH_EXP_ETYPE & 0x00FF);

  tx_buff[ETH_DATA_OFFSET] = 0;

  tx_buff_len = default_buf_len;
}

ISR(TIMER1_COMPA_vect)
{
  // Leer potenciometro
  uint16_t pot = analogRead(PIN_POT);
  //uint16_t pot = 555;

  // Enviar datos
  Serial.print(pot, HEX); Serial.println(" Send ISR POT");
  tx_buff[ETH_DATA_OFFSET + 0] = (pot & 0xFF00) >> 8;
  tx_buff[ETH_DATA_OFFSET + 1] = (pot & 0x00FF);
  tx_buff_len = default_buf_len + sizeof(pot);

  //tx_buff[ETH_DATA_OFFSET] = pot;
  w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
  w5500.execCmdSn(sckt, Sock_SEND_MAC);

}

void pin_ISR()
{
  if (debounce == true) {
    char msg[32];
    memset(msg, '\0', sizeof(msg));
    sprintf(msg, "Detectada pulsacion!");

    // Enviar datos
    Serial.println("Detectada pulsación del botón, enviando alarma");
    tx_buff_len = default_buf_len + sizeof(msg);

    for (int i = 0; i < sizeof(msg); i++) {
      tx_buff[ETH_DATA_OFFSET + i] = msg[i];
    }

    //tx_buff[ETH_DATA_OFFSET] = pot;
    w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
    w5500.execCmdSn(sckt, Sock_SEND_MAC);
    debounce = false;
  } else {
    debounce = true;
  }

}




////////////////////////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////////////////////////

void loop()
{
  while (true)
  {
    /**********************************************************************/
    // RX
    /**********************************************************************/

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

      // Print etype
      rx_etype = (rx_buff[ETH_ETYPE_OFFSET] << 8) | rx_buff[ETH_ETYPE_OFFSET + 1];
      sprintf(buff, "   ETYPE: 0x%x", rx_etype);
      Serial.println(buff);

      if (rx_etype == ETH_EXP_ETYPE)
      {
        print_eth_frame(rx_buff, rx_buff_len);
      }
    }

    /**********************************************************************/
    // WAIT
    /**********************************************************************/
    delay(20);
  }
}

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
