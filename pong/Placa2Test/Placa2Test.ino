#include <Ethernet2.h>
#include "EthRaw.h"

const static uint8_t numColum = 10;
const uint8_t tetriminos_I[8] = {3, 0, 4, 0, 5, 0, 6, 0};
const uint8_t tetriminos_J[8] = {3, 0, 3, 1, 4, 1, 5, 1};
const uint8_t tetriminos_L[8] = {5, 0, 3, 1, 4, 1, 5, 1};
const uint8_t tetriminos_O[8] = {4, 0, 5, 0, 4, 1, 5, 1};
const uint8_t tetriminos_S[8] = {5, 0, 6, 0, 4, 1, 5, 1};
const uint8_t tetriminos_T[8] = {5, 0, 4, 1, 5, 1, 6, 1};
const uint8_t tetriminos_Z[8] = {4, 0, 5, 0, 5, 1, 6, 1};

SOCKET sckt = 0;

const uint8_t my_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x11};
const uint8_t dst_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x13};


void print_eth_frame(uint8_t *frame, uint16_t frame_len);

uint8_t piecePos[PIECEPOS_LENGTH];
uint8_t board[BOARD_LENGTH];

////////////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);
  EthernetInit();
}

void EthernetInit() {
  w5500.init();
  w5500.writeSnMR(sckt, SnMR::MACRAW);
  w5500.execCmdSn(sckt, Sock_OPEN);
}
////////////////////////////////////////////////////////////////////////////////
//Funcion para generar tetrimino
////////////////////////////////////////////////////////////////////////////////
bool randomPiece() {
  //General un tetrimino aleatorio, si la posicion que se va generar ya esta ocupado,
  //entonces devolvera un false
  long randomNumber = random(7);
  switch (randomNumber) {
    case 0:
      return newPiece(tetriminos_I);
    case 1:
      return newPiece(tetriminos_J);
    case 2:
      return newPiece(tetriminos_Z);
    case 3:
      return newPiece(tetriminos_S);
    case 4:
      return newPiece(tetriminos_T);
    case 5:
      return newPiece(tetriminos_O);
    case 6:
      return  newPiece(tetriminos_L);
  }
}

bool newPiece(uint8_t* piecePosList) {
  //Genera tetrimino
  for (uint8_t i = 0; i < PIECEPOS_LENGTH; i += 2) {
    uint8_t idx = piecePosList[i] + piecePosList[i + 1] * numColum; //obtener indice segun la coordenada XY.
    if (board[idx] == 1) return false; //Si el esta posicion del tablero esta ocupado,entonces devuelve un false.
    piecePos[i] = piecePosList[i] ;
    piecePos[i + 1] = piecePosList[i + 1];
    //asignar las coordenadas.
    board[idx] = 1; //cambiar el estado de la casilla de tablero
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////////////////////////

void initBoard() {
  for (int  i = 0; i < 170; i++) {
    board[i] = 0;
  }
}
void  loop()
{
  uint8_t key ;
  // tx buffer
  uint8_t  tx_buff[200];
  uint16_t tx_buff_len;
  uint16_t tx_eType;
  // rx buffer
  uint8_t  rx_buff[200];
  uint16_t rx_buff_len;
  uint16_t rx_etype;

  uint32_t t0;
  uint8_t turn = 0;
  char buff[30];
  bool finished = false;
  initBoard();
  randomPiece();



  // Prepare tx frame
  for (int i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_DST_OFFSET + i] = dst_mac[i];
  for (int i = 0; i < ETH_MAC_LENGTH; i++)  tx_buff[ETH_SRC_OFFSET + i] = my_mac[i];

  //eType
  tx_buff[ETH_ETYPE_OFFSET + 0] = 0xFE;
  tx_buff[ETH_ETYPE_OFFSET + 1] = 0xAA;

  while (true)
  {
    if (Serial.available() > 0)
    {
      /**********************************************************************/
      // TX
      /**********************************************************************/
      key = Serial.read();
      if (!finished) {
        tx_buff[ETH_DATA_Key_OFFSET] = key;
        for (int x = 0; x < 8; x++) {
          tx_buff[ETH_DATA_PIECE + x ] = piecePos[x];
        }

        for (int i = 0; i < 170; i++) {
          tx_buff[ETH_DATA_BOARD + i] = board[i];
        }
        //Serial.println("Send ");
        //Serial.println(tx_buff);
        //print_eth_frame(tx_buff, tx_buff_len);
        tx_buff_len = ETH_MAC_LENGTH + ETH_MAC_LENGTH + ETH_ETYPE_LENGTH + BOARD_LENGTH + PIECEPOS_LENGTH + 1;
        w5500.send_data_processing(sckt, tx_buff, tx_buff_len);
        w5500.execCmdSn(sckt, Sock_SEND_MAC);
      }
    }
    /**********************************************************************/
    // RX
    /**********************************************************************/
    if (w5500.getRXReceivedSize(sckt) != 0)
    {

      w5500.recv_data_processing(sckt, rx_buff, 2);
      w5500.execCmdSn(sckt, Sock_RECV);

      rx_buff_len = rx_buff[0] << 8 | rx_buff[1];
      rx_buff_len -= 2;

      w5500.recv_data_processing(sckt, rx_buff, rx_buff_len);
      w5500.execCmdSn(sckt, Sock_RECV);
      rx_etype = (rx_buff[ETH_ETYPE_OFFSET] << 8) | rx_buff[ETH_ETYPE_OFFSET + 1];
      if (rx_etype == 0xFFFF) {
        for (int i = 0; i < 170; i++)
        {
          Serial.write(Null);
        }
        Serial.write('\n');
        finished = true;
      }
      if (!finished) {
        // Print etype
        //Serial.println("Receive ");
        //print_eth_frame(rx_buff, rx_buff_len);
        parseResponse(rx_buff);
      }
    }
  }
}

void parseResponse(uint8_t rx_buff[]) {
  for (int i = 0; i < 8; i++) {
    piecePos[i] = rx_buff[14 + i];
  }

  for (int i = 0; i < 170; i++)
  {
    board[i] = rx_buff[22 + i];
    Serial.write(rx_buff[22 + i]);
  }
  Serial.write('\n');
}
/*uint16_t randomPiece() {
  piecePos[0] = 0;
  piecePos[1] = 0;
  board[0] = 1;
  piecePos[2] = 1;
  piecePos[3] = 0;
  board[1] = 1;
  piecePos[4] = 2;
  piecePos[5] = 0;
  board[2] = 1;
  piecePos[6] = 3;
  piecePos[7] = 0;
  board[3] = 1;
  return 0x0000;
  }*/

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
    sprintf(buff, "%02x", frame[ETH_DATA_Key_OFFSET + i]);
    Serial.print(buff);

    if      ((i + 1) % 16 == 0)      Serial.print("\n         ");
    else if ((i + 1) % 8  == 0)      Serial.print(" ");
    else if (i < frame_len - 14 - 1) Serial.print(":");
  }

  Serial.println("");
}