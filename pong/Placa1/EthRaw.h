#ifndef ETHRAW_H
#define ETHRAW_H

// Field size
#define ETH_MAC_LENGTH 6
#define ETH_ETYPE_LENGTH 2
#define ETH_MAX_FRAME_SIZE 1514

// Offset to fields
#define ETH_DST_OFFSET   0
#define ETH_SRC_OFFSET   6
#define ETH_ETYPE_OFFSET 12
#define ETH_DATA_OFFSET  14

#define BCAST_MAC ((uint8_t[]){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})

#define ETH_EXP_ETYPE 0x88B5

#endif /* ETHRAW_H */
