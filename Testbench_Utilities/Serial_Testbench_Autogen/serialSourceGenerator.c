/*
    Serial Source Testbench
    Joseph A. De Vico
    5/17/2023

    Sometimes it's really annoying to create testbenches
    where one must simulate the transmission of serial
    data from some source. A common example being testing
    a system dependent on SPI commands, and being largely
    unable to quickly create the SPI MOSI signal on demand.

    This tool will autogenerate a .mem file and the associated
    testbench code that can easily be copied into your own
    file.

    Options are required for real output.

    Options:
        -p      Protocol    (more in future, these for now)
                    u(art)
                    s(pi)

        -f      Format
                    Uart:
                        {5-8}{N/E/O}{1/2}

                    Spi:
                        0-3 (Mode)

        -d      Data
                    hex string of data

        -D      Data from file
                    file path

        -b      Baudrate
                    base 10 baudrate, bits / s

        -w      Data Width for external file
                    integer number of BITS not bytes


        -T      Generate testbench file

        -P      Pause Bits
                    Number of bits to stall between data frames
                    before sending next frame

        Protocol option MUST come first, inline data must not be last argument
            if this is desired please terminate the data input with a " -"
            to indicate a field stop

    Example:

        ./serialSourceGen -p uart -f 8N1 -d 0x01 0x02 0x03 0x04 0x80 -b 500000 -M -T
*/


#include <stdio.h>
#include <stdint.h>
#include <string.h>     // Strlen
#include <stdlib.h>
#include "sersrcgenconstants.h"

//#define DEBUG_OUTPUT

#define MINARGS     4
#define ISARG(a)    ((a == '-') ? 1 : 0)
#define MAX(a,b)    ((a > b) ? a : b)

#define NEWLINE     '\n'

#define PROTOCOL    'p'
#define FORMAT      'f'
#define DATA_WIDTH  'w'
#define DATA_RAW    'd'
#define DATA_FILE   'D'
#define BAUD        'b'
#define GEN_TB      'T'
#define PAUSEBITS   'P'

struct EXTFILE_IO{
    uint8_t *data_buffer;
    uint16_t data_length;
} EXTFILE_IO;


uint8_t is_valid_protocol(char *input_str);
uint8_t fmt_create(uint8_t protocol_type, char *input_str);
uint8_t fmt_uart(char *input_str);

uint8_t handle_external_data(struct EXTFILE_IO *file_params, char *filepath, const uint8_t basesel, uint8_t d_width);

void serializer(uint8_t *rules, uint32_t baud, uint8_t *data_src, uint16_t data_len, uint8_t opt, uint32_t pause_bits);
uint16_t uart_mem_gen(FILE *fp, uint8_t fmt_rules, uint8_t *data_src, uint16_t data_len, uint32_t pause_bits);
void generate_tb(FILE *fp, uint8_t protocol, uint32_t delay_ns, uint16_t values_written);

// Struct transition in the future, this is getting too big for how it is now
/*
struct {
    uint16_t values_written;
    uint16_t data_bytes_written;
    uint16_t padding_bits_written;
} GENVALS;
*/

void main(int argc, char **argv){
    if(argc < MINARGS){
        printf("Invalid number of arguments.\n");
        return;
    }
#ifdef DEBUG_OUTPUT
    else printf("%3u Arguments Provided\n", argc);

#endif // DEBUG_OUTPUT

    uint8_t     state_select[MINARGS] = {0x00}; // State parameters that drive generation
    uint8_t     *data_set;                  // Pointer to dynamic sample array to serialize
    uint16_t    data_count = 0;             // Number of data fields in file or inline

    uint8_t     endianness = LITTLE_ENDIAN; // Default data format, will be variable someday
    uint32_t    baudrate = 1;   // Bits per second, plays into testbench delays

    uint8_t     OPT = 0;        // Testbench generation and more here
    uint32_t    PAUSE = 0;      // Pause between data frames in units of bits

    uint8_t     DATAWIDTH = 8;  // Default 8 bit width vals

    for(uint32_t n = 1; n < MAX(MINARGS, argc); n++){
        if(ISARG(argv[n][0])){
 #ifdef DEBUG_OUTPUT
            printf("Argument Detected:\t%s\n", argv[n]);
 #endif // DEBUG_OUTPUT
            switch(argv[n][1]){
                case PROTOCOL:
                    state_select[PROTOCOL_PTR] = is_valid_protocol(argv[++n]);
#ifdef DEBUG_OUTPUT
                    printf("Selected Protocol: %c\n", state_select[PROTOCOL_PTR]);
#endif
                break;

                case FORMAT:
                    state_select[FORMAT_PTR] = fmt_create(state_select[PROTOCOL_PTR], argv[++n]);
                break;

                case DATA_WIDTH:
                    if(n < argc) DATAWIDTH = (uint8_t)strtol(argv[++n], NULL, 10);

                    if(DATAWIDTH == 8
                       || DATAWIDTH == 16
                       || DATAWIDTH == 24
                       || DATAWIDTH == 32){
                       } else {
                            printf("Invalid data width provided. Defaulting to 8 bits.\n");
                            DATAWIDTH = 8;
                       }
                break;

                case DATA_RAW:{
#ifdef DEBUG_OUTPUT
                    printf("Inline data source\n");
#endif // DEBUG_OUTPUT
                    state_select[DATA_SRC_PTR] = DATA_INLINE;
                    uint8_t tmp_ptr = n + 1;
                    // Find size we need for malloc
                    while(!ISARG(argv[tmp_ptr][0])){
                        data_count += 1;

                        uint32_t val_bounds_check;

                        if(argv[n + 1 + tmp_ptr][0] == 'x' || argv[n + 1 + tmp_ptr][0] == 'X'
                                || argv[n + 1 + tmp_ptr][1] == 'x' || argv[n + 1 + tmp_ptr][1] == 'X'){

                            val_bounds_check = (uint32_t)strtol(             \
                                                    &argv[tmp_ptr][0],      \
                                                    NULL,                   \
                                                    16);
                        } else {
                            val_bounds_check = (uint32_t)strtol(             \
                                                    &argv[tmp_ptr][0],      \
                                                    NULL,                   \
                                                    10);                    \
                        }

                        // If user value is larger than X bits
                        if(val_bounds_check > 0x000000FF){
                            data_count += 1;
                        }

                        if(val_bounds_check > 0x0000FFFF){
                            data_count += 1;
                        }

                        if(val_bounds_check > 0x00FFFFFF){
                            data_count += 1;
                        }

                        tmp_ptr += 1;

                        if(data_count > MAX_DIN_CT){
                            state_select[DATA_SRC_PTR] = RETURN_ERROR;
                            break;
                        }
                    }

                    // If data is valid, allocate ram and fill
                    //  this dynamically sizes transmissions to the value
                    //  the user has entered. NOT fixed width unless all data
                    //  is within the same bit width
                    if(state_select[DATA_SRC_PTR] != RETURN_ERROR){
                        data_set = (uint8_t *)malloc(data_count * sizeof(uint8_t));

                        for(uint16_t m = 0; m < data_count; m++){
                            uint32_t user_val;
                            if(argv[n + 1 + m][0] == 'x' || argv[n + 1 + m][0] == 'X'
                                || argv[n + 1 + m][1] == 'x' || argv[n + 1 + m][1] == 'X'){

                                user_val = (uint32_t)strtol(argv[n + 1 + m], NULL, 16);
                            } else {
                                user_val = (uint32_t)strtol(argv[n + 1 + m], NULL, 10);
                            }

                            if(endianness == LITTLE_ENDIAN){
                                data_set[m] = (uint8_t)(user_val & 0xFF);
#ifdef DEBUG_OUTPUT
                                printf("Data[%2u] = 0x%02X\n", m, data_set[m]);
#endif // DEBUG_OUTPUT
                                if(user_val > 0x000000FF){
                                    data_set[++m] = (uint8_t)((user_val >> 8) & 0xFF);
#ifdef DEBUG_OUTPUT
                                printf("Data[%2u] = 0x%02X\n", m, data_set[m]);
#endif // DEBUG_OUTPUT
                                }

                                if(user_val > 0x0000FFFF){
                                    data_set[++m] = (uint8_t)((user_val >> 16) & 0xFF);
#ifdef DEBUG_OUTPUT
                                printf("Data[%2u] = 0x%02X\n", m, data_set[m]);
#endif // DEBUG_OUTPUT
                                }

                                if(user_val > 0x00FFFFFF){
                                    data_set[++m] = (uint8_t)((user_val >> 24) & 0xFF);
#ifdef DEBUG_OUTPUT
                                printf("Data[%2u] = 0x%02X\n", m, data_set[m]);
#endif // DEBUG_OUTPUT
                                }
                            }


                        }
                    }

                    /*
                    // If valid data found
                    if(state_select[DATA_SRC_PTR] != RETURN_ERROR){
                        data_set = (char *)malloc(data_size * sizeof(char));
                        tmp_ptr = n + 1;
                        uint16_t tmp_str_pt = 0;
                        uint16_t tmp_data_pt = 0;

                        // Copy input strings into buffer
                        for(uint16_t m = 0; m < data_count;){
                            if(argv[m + tmp_ptr][tmp_str_pt]){
                                data_set[tmp_data_pt++] = argv[m + tmp_ptr][tmp_str_pt++];
                            } else {
                                data_set[tmp_data_pt++] = NEWLINE;
                                tmp_str_pt = 0;
                                m += 1;
                            }

                        }

                    }
                    */
                }
                break;

                case DATA_FILE:{
                    state_select[DATA_SRC_PTR] = DATA_EXTERNAL;
                    struct EXTFILE_IO *file_data;
                    file_data = (struct EXTFILE_IO *)malloc(sizeof(EXTFILE_IO));


                    //uint8_t handle_external_data(struct EXTFILE_IO *file_params, char *filepath, const uint8_t basesel, uint8_t d_width){
                    if(n < argc){
                        if(handle_external_data(file_data, argv[++n], 16, DATAWIDTH)){
                            printf("File handling error!!\n");
                        } else {
                            data_set = file_data->data_buffer;
                            data_count = file_data->data_length;
                        }
                    } else {
                        printf("No file path provided!\n");
                    }


                    free(file_data);
                }
                break;

                case BAUD:
                    baudrate = (uint32_t)strtol(argv[++n], NULL, 10);
#ifdef DEBUG_OUTPUT
                    printf("Baudrate Selected: %u\n", baudrate);
#endif // DEBUG_OUTPUT
                break;

                case GEN_TB:
#ifdef DEBUG_OUTPUT
                    printf("Testbench Generation Enabled.\n");
#endif // DEBUG_OUTPUT
                    OPT |= GENERATE_TB;
                break;

                case PAUSEBITS:
                    if(n < argc) PAUSE = (uint32_t)strtol(argv[++n], NULL, 10);
                break;
            }
        }
    }

    // Verify nothing weird on user entry
    for(uint16_t n = 0; n < MINARGS; n++){
        if(state_select[n] == RETURN_ERROR){
            printf("Error on ");
            switch(n){
                case PROTOCOL_PTR:
                    printf("Protocol");
                break;

                case FORMAT_PTR:
                    printf("Format");
                break;

                case DATA_SRC_PTR:
                    printf("Data");
                break;
            }

            printf(" Entry. Please try again.\n");
            return;
        }
    }

    //void serializer(uint8_t *rules, uint32_t baud, uint8_t *data_src, uint16_t data_len, uint8_t opt){
    serializer(state_select, baudrate, data_set, data_count, OPT, PAUSE);


    if(state_select[DATA_SRC_PTR] != RETURN_ERROR){
        free(data_set);
    }

    printf("All done :^)\n");

}   // END MAIN

/////////////////////////////////////////////////////////////////////////////
// Check if protocol requested is supported
uint8_t is_valid_protocol(char *input_str){
#ifdef DEBUG_OUTPUT
    printf("FUNCTION MESSAGE: Protocol Check String = %s\n", input_str);
#endif // DEBUG_OUTPUT
    uint8_t retval;
    switch(input_str[0]){
        case PROTOCOL_UART:
            retval = PROTOCOL_UART;
        break;
        case PROTOCOL_SPI:
            retval = PROTOCOL_SPI;
        break;
        default:
            retval = RETURN_ERROR;
        break;
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////
// Establish format rules for serialization later
uint8_t fmt_create(uint8_t protocol_type, char *input_str){
    uint8_t retval;
#ifdef DEBUG_OUTPUT
    printf("FUNCTION MESSAGE: Format for %c\n", protocol_type);
#endif // DEBUG_OUTPUT
    switch(protocol_type){
        case PROTOCOL_UART:
            retval = fmt_uart(input_str);
        break;

        case PROTOCOL_SPI:
            printf("Unsupported right now! :(\n");
            retval = RETURN_ERROR;
        break;

        default:
            retval = RETURN_ERROR;
        break;
    }
    return retval;
}

// UART Format Byte Structure:
//  7   6   5   4   3   2   1   0
//  E   S   P   P   D   D   D   D
//
//      E: Endianness
//          0: Little Endian (dfl)
//          1: Big Endian
//      S: Stop bit count (0, 1)
//          0: 1 stop bits
//          1: 2 stop bits
//      P: Parity
//          0: None
//          1: Odd
//          2: Even
//      D: Data Bits (0-9)
uint8_t fmt_uart(char *input_str){
    uint8_t retval;

#ifdef DEBUG_OUTPUT
    printf("FUNCTION MESSAGE: UART Format\t%s\n", input_str);
#endif // DEBUG_OUTPUT

    if(input_str[0] > ('5' - 0x01) && input_str[0] < ('9' + 0x01)){
        retval = (input_str[0] - '0') & 0x0F;
    } else {
        retval = RETURN_ERROR;
    }

    switch(input_str[1]){
        case 'N':
            retval |= UART_N_PARITY;
        break;
        case 'O':
            retval |= UART_O_PARITY;
        break;
        case 'E':
            retval |= UART_E_PARITY;
        break;
    }

    if(input_str[2] == '2'){
        retval |= UART_2_STOP;
    } else {
        retval |= UART_1_STOP;
    }

    return retval;
}
/////////////////////////////////////////////////////////////////////////////
// Handle external file input, parse, and allocation
//  account for different bases of data
uint8_t handle_external_data(struct EXTFILE_IO *file_params, char *filepath, const uint8_t basesel, uint8_t d_width){
    uint8_t retval = 0;
    FILE *extfp;

    extfp = fopen((const char *)filepath, "r");

    if(!extfp){
        printf("FUNCTION MESSAGE: Provided File Path does not exist!\n");
        retval = RETURN_ERROR;
    } else {
        char linebuffer[MAX_DIN_CHAR_LEN];
        uint8_t read_len;
        int32_t tmp_rd_val;
        uint16_t file_line_ct;
        uint16_t dynarr_wp = 0;

        // Get line len of file
        char testchar = getc(extfp);
        while(testchar != EOF){
            if(testchar == '\n') file_line_ct += 1;
            testchar = getc(extfp);
        }

        printf("Data Width of %u\n", d_width);
        printf("Read %u lines from %s\n", file_line_ct, filepath);


        rewind(extfp);

        file_params->data_buffer = (uint8_t *)malloc((d_width >> 3) * file_line_ct);
        file_params->data_length = (d_width >> 3) * file_line_ct;
#ifdef DEBUG_OUTPUT
        printf("Allocated %u bytes for data\n", file_params->data_length);
#endif
        if(file_params->data_buffer){
            if(basesel == 10){
                for(uint16_t n = 0; n < file_line_ct; n++){
                    read_len = fscanf(extfp, "%d", &tmp_rd_val);

                    uint8_t *tv = (uint8_t *)(&tmp_rd_val);
                    file_params->data_buffer[dynarr_wp++] = tv[0];

                    if(d_width > 8){
                        file_params->data_buffer[dynarr_wp++] = tv[1];
                    }

                    if(d_width > 16){
                        file_params->data_buffer[dynarr_wp++] = tv[2];
                    }

                    if(d_width > 24){
                        file_params->data_buffer[dynarr_wp++] = tv[3];
                    }

                }
            } else {
                for(uint16_t n = 0; n < file_line_ct; n++){
                    read_len = fscanf(extfp, "%x", &tmp_rd_val);

                    uint8_t *tv = (uint8_t *)(&tmp_rd_val);
                    file_params->data_buffer[dynarr_wp++] = tv[0];

                    if(d_width > 8){
                        file_params->data_buffer[dynarr_wp++] = tv[1];
                    }

                    if(d_width > 16){
                        file_params->data_buffer[dynarr_wp++] = tv[2];
                    }

                    if(d_width > 24){
                        file_params->data_buffer[dynarr_wp++] = tv[3];
                    }
                }
            }
        } else {
            printf("FUNCTION MESSAGE: File Buffer Dynamic Allocation Failed. :(\n");
            retval = RETURN_ERROR;
        }


    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////
// Actually create output serial data stream and
//  associated testbench driver code.

void serializer(uint8_t *rules, uint32_t baud, uint8_t *data_src, uint16_t data_len, uint8_t opt, uint32_t pause_bits){
    FILE *memfile;
    FILE *tb_file;

    const char *output_tb_name;

    const char dfl_tb_name[] = "testbench_boilerplate.v";
    const char uart_tb_name[] = "UART_Source_Module.v";

    memfile = fopen("serialized_data.mem", "w");

    uint16_t serialized_vals = 0;
    uint32_t baud_delay_ns;

    baud_delay_ns = (uint32_t)((double)1000000000.0 * (1.0 / (double)baud));

    switch(rules[PROTOCOL_PTR]){
        case PROTOCOL_UART:
            serialized_vals = uart_mem_gen(memfile, rules[FORMAT_PTR], data_src, data_len, pause_bits);
            output_tb_name = uart_tb_name;
        break;

        default:
            printf("Unrecognized protocol input!\n");
            output_tb_name = dfl_tb_name;
        break;
    }

    if(serialized_vals && (opt & GENERATE_TB)){
        tb_file = fopen(output_tb_name, "w");
        generate_tb(tb_file, rules[PROTOCOL_PTR], baud_delay_ns, serialized_vals);
        fclose(tb_file);
    }

#ifdef DEBUG_OUTPUT
    printf("FUNCTION MESSAGE: Serialization Complete!\n");
#endif // DEBUG_OUTPUT

    fclose(memfile);
}

// .mem generators return the number of written bytes
//  eg the number of serial bit events present
// UART .mem generator
uint16_t uart_mem_gen(FILE *fp, uint8_t fmt_rules, uint8_t *data_src, uint16_t data_len, uint32_t pause_bits){
    uint16_t bytes_written = 0;

    uint8_t data_bit_ct = fmt_rules & 0x0F;
    uint8_t parity_type = fmt_rules & (0x03 << 4);
    uint8_t stop_bit_ct = (fmt_rules >> 6 & 0x01);
#ifdef DEBUG_OUTPUT
    printf("FUNCTION MESSAGE: %u Data Bits\n", data_bit_ct);
    printf("FUNCTION MESSAGE: %u Stop Bits\n", stop_bit_ct);

#endif // DEBUG_OUTPUT


    if(fmt_rules & UART_BIG_ENDIAN){

    } else {
        for(uint16_t n = 0; n < data_len; n++){
            uint8_t parity_chk_accum = 0;       // Acuumulate ones
            fprintf(fp, "0\n");                 // Start bit
            bytes_written += 1;

            for(uint16_t m = 0; m < data_bit_ct; m++){
                char tbw = ((data_src[n] & (1 << m)) ? '1' : '0');
                if(tbw == '1') parity_chk_accum += 1;
                fprintf(fp, "%c\n", tbw);         // Data Bits
                bytes_written += 1;
                //if(!(m == (data_bit_ct - 1) && n == data_len - 1)) fprintf(fp, "\n");
            }

            // Parity Checking
            if(parity_type != UART_N_PARITY){
                if((parity_type == UART_O_PARITY) && (parity_chk_accum & 0x01)){
                    fprintf(fp, "1\n");
                    bytes_written += 1;
                }
                else if(parity_type == UART_E_PARITY && !(parity_chk_accum & 0x01)){
                    fprintf(fp, "1\n");
                    bytes_written += 1;
                } else {
                    fprintf(fp, "0\n");
                    bytes_written += 1;
                }
            }



            // Stop Bits
            if(stop_bit_ct){
                fprintf(fp, "1\n1");
                bytes_written += 2;
            } else {                // Single stop bit
                fprintf(fp, "1");
                bytes_written += 1;
            }

            // Write bits between data frames
            if(pause_bits){
                for(uint32_t q = 0; q < pause_bits; q++){
                    fprintf(fp, "\n1");
                    bytes_written += 1;
                }
            }

            if(!(n == data_len - 1)) fprintf(fp, "\n");
        }
    }

    return bytes_written;
}


// Boilerplate testbench generation
void generate_tb(FILE *fp, uint8_t protocol, uint32_t delay_ns, uint16_t values_written){
    char START_VAL = '0';

    fprintf(fp, "// This module has been autogenerated\n");
    fprintf(fp, "// so you likely will need to change how\n");
    fprintf(fp, "// this works. Good luck! :)\n");

    fprintf(fp, "\nmodule ");

    switch(protocol){
        case PROTOCOL_UART:
            fprintf(fp, "UART_source");

            START_VAL = '1';
        break;
    }

    fprintf(fp, "\n(\n");

    // When synchronous option is here, clk input
    //if()

    fprintf(fp, "\toutput reg SERIAL_STREAM\n\t,output reg BAUD_CLK\n);");
    fprintf(fp, "\n\t// Bitstream length");
    fprintf(fp, "\n\tlocalparam SERIALIZED_LEN = %u;\n", values_written);
    fprintf(fp, "\n\tinteger n;\n\treg serialized_values[0:%u];\n", values_written - 1);
    fprintf(fp, "\n\tinitial begin\n\t\tn = 0;\n\t\tBAUD_CLK = 0;\n\t\tSERIAL_STREAM = %c;\n", START_VAL);
    fprintf(fp, "\t\t$readmemh(\"serialized_data.mem\", serialized_values);\n\n");

    fprintf(fp, "\t\t#%u;\t//Startup Delay of 1 BAUD period\n", delay_ns);

    fprintf(fp, "\n\t\tforever begin\n\t\t\t");
    fprintf(fp, "SERIAL_STREAM <= serialized_values[n];\n\t\t\t");
    fprintf(fp, "if(n < SERIALIZED_LEN - 1) n <= n + 1;\n\t\t\t");
    fprintf(fp, "else n <= 0;\n\t\t\t");
    fprintf(fp, "BAUD_CLK <= 1;\n\t\t\t");
    fprintf(fp, "#%u;\t// ns, This determines your baudrate\n\t\t\t", delay_ns >> 1);
    fprintf(fp, "BAUD_CLK <= 0;\n\t\t\t");
    fprintf(fp, "#%u;\t// ns, This determines your baudrate\n\t\tend\n", delay_ns >> 1);



    fprintf(fp, "\t\n\tend\nendmodule");

}




