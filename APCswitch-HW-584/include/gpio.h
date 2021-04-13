//******************************************************************************
//                              G P I O . H                                    *
//******************************************************************************
// GPIIO related functions.                                                    *
//******************************************************************************
#ifndef __GPIO_H__
#define __GPIO_H__
#include <stm8s.h>

#define RESETBUTTON (ValBit(GPIOA->IDR,1)==0)        // Input from "RST" button
#define ENABLEOUTPUT (SetBit(GPIOG->ODR,1)==0)       // G1 output is enable signal

void     clk_init() ;                                // Initialize CPU clock
void     gpio_init() ;                               // Initialize GPIO
void     IWDG_init() ;                               // Initialize IWDG watchdog
void     IWDG_Reload() ;                             // Feed the watchdog
void     delay_us ( uint16_t us ) ;                  // Delay number of microseconds
void     delay_ms ( uint16_t ms ) ;                  // Delay number of milliseconds
void     delay_ms_wd ( uint16_t ms ) ;               // Delay_ms while feeding the watchdog
uint16_t delay_timer0() ;                            // Get value of free running timer
uint32_t seconds() ;                                 // Get value of free running seconds timer
bool     periodic_timer_expired() ;                  // Check periodic timer
bool     arp_timer_expired() ;                       // Check arp timer
void     LEDset ( uint8_t onoff ) ;                  // Turn LED on or off
void     outlet_set ( uint8_t chn, uint8_t onoff ) ; // Set or clear the output bit
uint8_t  input_read ( uint8_t chn ) ;                // Read an input bit
void     flash_write_enable ( uint8_t enable ) ;     // Enable/disable flash write
void     eeprom_write_enable ( uint8_t enable ) ;    // Enable/disable EEPROM write
void     serial_init() ;                             // Init serial output
void     serial_write ( char data ) ;                // Send char to UART2
void     serial_write_str ( const char* data ) ;     // Print string to UART2
void     serial_write_str_lf ( const char* data ) ;  // Print string to UART2
void     serial_write_hex ( const char* data,        // Print hexadecimal shorts
                            uint16_t h ) ;
void     serial_dump_hex ( const uint8_t* data,      // Hex dump array of bytes
                           uint16_t len ) ;
extern   uint16_t pwok ;                             // Password okay

#endif /* __GPIO_H__ */
