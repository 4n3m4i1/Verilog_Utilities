# Serial Testbench Autogenerator
This tool will generate a `.mem` file and  
associated testbench code snippit to automatically  
serialize some input data set.  
  
This assists in the development of testbenches that  
depend on the testbench providing a serial data stream  
to the modules under test.  
  
In short, this makes your testbench source the requisite  
bitstream required to emulate a UART, SPI, CAN, etc..  
stream.  
  
  
# Protocol Support
- UART (5-9 N/E/O 1/2)
- [PLANNED] SPI (Mode 0 - 3)

## Future Support
- [PLANNED, Long Term] CAN
- [PLANNED, Long Term] i2C
  

# Command Line Syntax
- `-p` Protocol Selection
    - uart (only one supported rn)
  
- `-d` Inline data to be serialized (hex -> 0xVALUE or base 10)
  
- `-f` Format, protocol specific
    - example: 8N1, 8O2, 7E2m etc.. (this is case sensitive!!)
  
- `-D` Data from some file to be serialized
    - Not supported fully at the moment
      
- `b` Baudrate, integer base 10 format (bits / s)
  
- `T` Generate Testbench snippit code  
    - `.mem` is generated automatically, testbench must be selected
    - The baudrate selected will be present in the form of `ns` delays 
  

## Command Line Example
`./serialSourceGenerator -p uart -d 0xAA 0x55 0xFF 0x81 -f 8N1 -b 500000 -T`  
Will generate a `.mem` file containing the serialized form of  
the inline data bytes provided (0xAA 0x55 0xFF 0x81) in little endian  
8N1 format, a testbench is generated that matches 500k baud output.

