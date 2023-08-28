#ifndef SERSRCGENCONSTANTS_H
#define SERSRCGENCONSTANTS_H



#define RETURN_ERROR    0xFF

#define ___LITTLE_ENDIAN___   0x00
#define ___BIG_ENDIAN___      0x01

#define PROTOCOL_PTR    0
#define PROTOCOL_UART   'u'
#define PROTOCOL_SPI    's'
#define PROTOCOL_i2C    'i'
#define PROTOCOL_CAN    'c'

#define FORMAT_PTR      1
#define UART_BIG_ENDIAN (1 << 7)
#define UART_N_PARITY   (0 << 4)
#define UART_O_PARITY   (1 << 4)
#define UART_E_PARITY   (2 << 4)

#define UART_1_STOP     (0 << 6)
#define UART_2_STOP     (1 << 6)


#define DATA_SRC_PTR    2
#define MAX_DIN_CT      16
#define MAX_DIN_CHAR_LEN    16
#define DATA_INLINE     (0 << 7)
#define DATA_EXTERNAL   (1 << 7)


#define GENERATE_TB     (1 << 0)







#endif
