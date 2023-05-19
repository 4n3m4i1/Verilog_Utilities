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
    - example: 8N1, 8O2, 7E2 etc.. (this is case sensitive!!)
  
- `w` Data Width in bits for external file data source
    - Valid: 8 (default), 16, 24, 32

- `-D` Data from some file to be serialized
    - Should be hex values seperated by a newline
    - Example `-D file_in_execution_directory.txt`

- `b` Baudrate, integer base 10 format (bits / s)
  
- `T` Generate Testbench snippit code  
    - `.mem` is generated automatically, testbench must be selected
    - The baudrate selected will be present in the form of `ns` delays 

- `P` Pause Bits between data frames
    - The default value is zero, such that the next start condition is the  
        bit after the stop condition.
  

## Command Line Example
`./serialSourceGenerator -p uart -d 0xAA 0x55 0xFF 0x81 -f 8N1 -b 500000 -T -P 10`  
Will generate a `.mem` file containing the serialized form of  
the inline data bytes provided (0xAA 0x55 0xFF 0x81) in little endian  
8N1 format, a testbench is generated that matches 500k baud output.  
There will be a 10 bit delay (10 * 1/BAUDRATE seconds) between data frames  
(eg. last STOP bit -> 10 bits idle -> next START)  
  

### Example Using File Data Sources
A file named `testVectors.mem` containing the following:  
```
0xAA
55
22
```
Can be processed via the following:  
`./serialSourceGen -p uart -w 8 -D testVectors.mem -f 8N1 -b 500000 -T`  
Note: when using 8 bit datatypes specifying a `-w` width is optional.  
Note: The width specifier must be present before the `-D` file call
  


