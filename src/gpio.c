//******************************************************************************
//                              G P I O . C                                    *
//******************************************************************************
// GPIIO related functions.                                                    *
//******************************************************************************
#include <string.h>
#include "gpio.h"

#define	FLASH_RASS_KEY1   0x56            // For writing to flash
#define FLASH_RASS_KEY2   0xAE
#define	EEPROM_RASS_KEY1  0xAE            // For writing to EEPROM
#define EEPROM_RASS_KEY2  0x56

struct IO_t                               // For translation to GPIO and bit
{                                         // Index is channel (0..7)
  uint8_t*  gpio ;                        // GPIO base address to use
  uint8_t   bit ;                         // Bit to use
} ;

struct IO_t relais[] =                    // Table with outputs to relais
     {
       { GPIOC->ODR, 7 },                 // Relais/outlet 1  pin 8
       { GPIOA->ODR, 4 },                 // Relais/outlet 2  pin 9
       { GPIOD->ODR, 7 },                 // Relais/outlet 3  pin 10
       { GPIOD->ODR, 3 },                 // Relais/outlet 4  pin 12
       { GPIOD->ODR, 0 },                 // Relais/outlet 5  pin 13
       { GPIOE->ODR, 3 },                 // Relais/outlet 6  pin 14
       { GPIOG->ODR, 0 },                 // Relais/outlet 7  pin 15
       { GPIOC->ODR, 6 }                  // Relais/outlet 8  pin 16
     } ;

struct IO_t inputs[] =                    // Table with inputs
     {
       { GPIOA->IDR, 3 },                 // Input on pin 1
       { GPIOA->IDR, 5 },                 // Input on pin 2
       { GPIOD->IDR, 4 },                 // Input on pin 4
       { GPIOD->IDR, 2 },                 // Input on pin 5
       { GPIOE->IDR, 0 },                 // Input on pin 6
       { GPIOG->IDR, 1 }                  // Input on pin 7
     } ;


uint16_t timer0 = 0 ;                     // Extra free running timer
uint16_t msecdelay = 100 ;                // Time to delay in msec
uint32_t second_counter = 0 ;             // Free running second counter
uint8_t  periodic_timer= 0 ;              // TCP/IP timer
uint8_t  arp_timer= 0 ;                   // arp timer
uint16_t pwok ;                           // Timer for password okay



//******************************************************************************
//                            G P I O _ I N I T                                *
//******************************************************************************
//  Setup the system clock to run at 16MHz using the internal oscillator.      *
//******************************************************************************
void clk_init()
{
  //CLK->ICKR |= ((uint8_t)0x01); // Enable HSI oscillator (MAY NOT BE NECESSARY AFTER RESET)
  //CLK->CKDIVR &= ~CLK_CKDIVR_HSIDIV ;
  CLK->SWCR |= CLK_SWCR_SWEN ;
  CLK->CKDIVR = 0 ;
}


//******************************************************************************
//                            G P I O _ I N I T                                *
//******************************************************************************
// Initialize GPIO registers.                                                  *
// GPIO Definitions for 8+1 outputs, 2 RX/TX 5 INPUTS.                         *
// Relais outputs are open drain.                                              *
// Assumption is that power-on reset has set all GPIO to the default states    *
// of the chip hardware (see STM8 manuals).                                    *
//                                                                             *
// This function will set the GPIO registers to states for this specific       *
// application (Network Module). Only the GPIO bits that are attached to pins  *
// are manipulated here.                                                       *
//                                                                             *
// The STM8S is mostly used as a GPIO driver device. Most outputs drive        *
// devices such as relays. The SPI interface to the ENC28J60 chip is GPIO      *
// "bit bang" driven.                                                          *
//                                                                             *
// Any pins that are "not used" are set as pulled up inputs.                   *
//                                                                             *
// I/O pin  Port/Bit    IN/OUT    Description                                  *
// -------  ---------   ------    -----------------------------------------    *
//   1        A3        INPUTPU   20 pins connector                            *
//   2        A5        INPUTPU   20 pins connector                            *
//   3        D6        UART2_RX  20 pins connector                            *
//   4        D4        INPUTPU   20 pins connector                            *
//   5        D2        INPUTPU   20 pins connector                            *
//   6        E0        INPUTPU   20 pins connector                            *
//   7        G1        OUTPUT    20 pins connector Enable signal              *
//   8        C7        OUTPUTOD  20 pins connector Output to Relais 1         *
//   9        A4        OUTPUTOD  20 pins connector Output to Relais 2         *
//  10        D7        OUTPUTOD  20 pins connector Output to Relais 3         *
//  11        D5        UART2_TX  20 pins connector                            *
//  12        D3        OUTPUTOD  20 pins connector Output to Relais 4         *
//  13        D0        OUTPUTOD  20 pins connector Output to Relais 5         *
//  14        E3        OUTPUTOD  20 pins connector Output to Relais 6         *
//  15        G0        OUTPUTOD  20 pins connector Output to Relais 7         *
//  16        C6        OUTPUTOD  20 pins connector Output to Relais 8         *
//                                                                             *
// Other pins:                                                                 *
//          Port/Bit    IN/OUT    Description                                  *
//          ---------   ------    -----------------------------------------    *
//            A0        INPUTPU   Not used, not attached to pin                *
//            A1        INPUTPU   Reset button                                 *
//            A2        OUTPUT    LED                                          *
//            A6        INPUTPU   Not used                                     *
//            A7        INPUTPU   Not used, not attached to pin                *
//            B0..7     INPUTPU   Not used.                                    *
//            C0        INPUTPU   Not used, not attached to pin                *
//            C1        OUTPUTF   ENC28J60 CS                                  *
//            C2        OUTPUTF   ENC28J60 SCK                                 *
//            C3        OUTPUTF   ENC28J60 MISO                                *
//            C4        OUTPUT    ENC28J60 MOSI                                *
//            C5        OUTPUT    ENC28J60 INT                                 *
//            D1        INPUTPU   SWIM                                         *
//            E1        INPUTPU   Not used                                     *
//            E2        INPUTPU   Not used                                     *
//            E4        INPUTPU   Not used, not attached to pin                *
//            E5        OUTPUTF   ENC28J60 RESET                               *
//            E6        INPUTPU   Not used                                     *
//            E7        INPUTPU   Not used                                     *
//            F2        INPUTPU   Not used, not attached to pin                *
//            F3        INPUTPU   Not used, not attached to pin                *
//            F4        INPUTPU   Not used, not attached to pin                *
//            F5        INPUTPU   Not used, not attached to pin                *
//            F6        INPUTPU   Not used, not attached to pin                *
//            F7        INPUTPU   Not used, not attached to pin                *
//            G2        INPUTPU   Not used, not attached to pin                *
//            G3        INPUTPU   Not used, not attached to pin                *
//            G4        INPUTPU   Not used, not attached to pin                *
//            G5        INPUTPU   Not used, not attached to pin                *
//            G6        INPUTPU   Not used, not attached to pin                *
//            G7        INPUTPU   Not used, not attached to pin                *
//******************************************************************************
void gpio_init()
{
  GPIOA->DDR = 0x14 ;                     // PORT A all inputs except A2 and A4
  GPIOA->CR1 = 0xEF ;                     // All inputs PULL-UP, A2 PUSH-PULL, A4 open drain
  GPIOA->CR2 = 0x00 ;                     // 2MHz, Interrupt disable
  GPIOB->DDR = 0x00 ;                     // PORT B all inputs
  GPIOB->CR1 = 0xFF ;                     // All inputs PULL-UP
  GPIOB->CR2 = 0x00 ;                     // 2MHz, Interrupt disable
  GPIOC->DDR = 0xCE ;                     // C1, C2 and C3 ENC28J60 output, C6 and C7 output
  GPIOC->CR1 = 0x3F ;                     // C6 and C7 open drain
  GPIOC->CR2 = 0x0E ;                     // C1, C2, C3 outputs for ENC28J60 are fast mode
  GPIOD->DDR = 0x89 ;                     // D0, D3 and D7 output
  GPIOD->CR1 = 0x76 ;                     // D0, D3 and D7 open drain
  GPIOD->CR2 = 0x00 ;
  GPIOE->DDR = 0x28 ;                     // All input except E5 (ENC28J60 reset) and E3
  GPIOE->CR1 = 0xF7 ;                     // E3 open drain
  GPIOE->CR2 = 0x20 ;                     // Fast mode for ENC28J60 reset
  GPIOG->DDR = 0x43 ;                     // G0, G1 and G6 output
  GPIOG->CR1 = 0xFE ;                     // G0 open drain
  GPIOG->CR2 = 0x00 ;
  // Init Timer 2
  //TIM2->SR1 &= !TIM2_SR1_UIF ;
  TIM2->PSCR = 0x04 ;                     // TIM2_PRESCALER_16, run on 1 MHz
  TIM2->ARRH = 1000 >> 8 ;                // Reload value to 1000 for 1 msec
  TIM2->ARRL = 1000 & 0x00FF ;
  TIM2->CR1 |= TIM2_CR1_ARPE ;
  TIM2->IER |= TIM2_IER_UIE ;             // Enable Update Interrupt
  TIM2->CR1 |= TIM2_CR1_CEN ;             // Enable TIM2
}


//******************************************************************************
//                          I W D G _ R E L O A D                              *
//******************************************************************************
// Feed the watchdog.                                                          *
//******************************************************************************
void IWDG_Reload()
{
  IWDG->KR =  0xAA ;                                // IWDG_KEY_REFRESH 
}


//******************************************************************************
//                           I W D G I N I T                                   *
//******************************************************************************
// Initialize IWDG (Independent Watchdog).                                     *
// The IWDG will perform a hardware reset after 1 second if it is not          *
// serviced with a write to the IWDG_KR register - the main loop needs to      *
// perform the servicing writes.                                               *
//******************************************************************************
void IWDG_init()
{
  IWDG->KR = 0xCC ;                                // IWDG_KEY_ENABLE
  IWDG->KR = 0x55 ;                                // IWDG_WriteAccess_Enable
  IWDG->PR = 0x06 ;                                // IWDG_Prescaler_256
  IWDG->RLR = 0xFF ;                               // Countdown value
  IWDG_Reload() ;                                  // Feed the watchdog
}


//******************************************************************************
//                          O U T L E T _ S E T                                *
//******************************************************************************
// Control the outolet according the onoff parameter.                          *
// Parameters are the channel number (0..7) and the new state.                 *
// onoff = FALSE : output low, optocoupler active, relais on, power off        *
// onoff = TRUE : output high, optocoupler idle, relais off, power on          *
//******************************************************************************
void outlet_set ( uint8_t chn, uint8_t onoff )
{
  if ( onoff )                                       // ON requested?
  {
    //serial_write_str ( "Outlet Set " ) ;
    SetBit ( *relais[chn].gpio, relais[chn].bit ) ;  // Yes, set bit
  }
  else
  {
    //serial_write_str ( "Outlet Clr " ) ;
    ClrBit ( *relais[chn].gpio, relais[chn].bit ) ;  // No, clear bit
  }
  //serial_write_hex ( " GPIO at ", (uint16_t) relais[chn].gpio ) ;
}


//******************************************************************************
//                          I N P U T _ R E A D                                *
//******************************************************************************
// Read the input on a pin.  Channels are number 0..5, see comment on inputs[] *
// for the pin on the connector.                                               *
// Since the normal value is HIGH (Pull-up), TRUE is returned if value is LOW. *
//******************************************************************************
uint8_t input_read ( uint8_t chn )
{
  if ( ValBit ( *inputs[chn].gpio, inputs[chn].bit ) ) // Input bit set?
  {
    return FALSE ;                                     // Yes, return FALSE
  }
  return TRUE ;                                        // Else TRUE
}


//******************************************************************************
//                               L E D S E T                                   *
//******************************************************************************
// Control the LED according the onoff parameter.                              *
// LED is on A2.                                                               *
//******************************************************************************
void LEDset ( uint8_t onoff )
{
  if ( onoff )                              // ON requested?
  {
    SetBit ( GPIOA->ODR, 2 ) ;              // Yes, set LED bit
  }
  else
  {
    ClrBit ( GPIOA->ODR, 2 ) ;              // No, clear LED bit
  }
}


//******************************************************************************
//                           D E L A Y _ C Y C L                               *
//******************************************************************************
// Delay for "ticks" * 4 cycles.                                               *
//******************************************************************************
// Ignore warning for unused "ticks"
#pragma save
#pragma disable_warning 85
void delay_cycl ( uint16_t ticks )
{
	// ldw X, ticks ; insert automaticaly
	__asm__ ( "$N:\n decw X\n jrne $N\n" ) ;     // 4 cycle loop
}
#pragma restore


//******************************************************************************
//                    D E L A Y _ U S                                          *
//******************************************************************************
// Delay for "us" microseconds. "us" must be in the range 0..10000.            *
//******************************************************************************
void delay_us ( uint16_t us )
{
  delay_cycl ( us * 5 ) ;
}


//******************************************************************************
//                    D E L A Y _ M S                                          *
//******************************************************************************
// Delay for "ticks" milliseconds.                                             *
//******************************************************************************
void delay_ms( uint16_t ms )
{
  msecdelay = ms ;                        // Set delaycount
	while ( msecdelay )                     // Wait until zero again
  {
    wfi() ;                               // Wait for interrupts
  }
}


//******************************************************************************
//                    D E L A Y _ M S _ W D                                    *
//******************************************************************************
// Delay for "ticks" milliseconds, while feeding the watchdog.                 *
//******************************************************************************
void delay_ms_wd ( uint16_t ms )
{
	while ( ms-- )
	{
		delay_ms ( 1 ) ;                    // Delay for 1 msec
    if ( ( ms & 0xFF ) == 0 )           // Feed watchdog every 256 msec
    {
      IWDG_Reload() ;
    }
  }
}


//******************************************************************************
//             P E R I O D I C _ T I M E R _ E X P I R E D                     *
//******************************************************************************
// Check if periodic timer has expired (20 msec).                              *
//******************************************************************************
bool periodic_timer_expired()
{
	if ( periodic_timer < 20 )            // Check timer
  {
    return FALSE ;                      // Not expired
  }
  periodic_timer = 0 ;                  // Expired, reset
  return TRUE ;
}


//******************************************************************************
//             A R P _ T I M E R _ E X P I R E D                               *
//******************************************************************************
// Check if arp timer has expired (10 seconds).                                *
//******************************************************************************
bool arp_timer_expired()
{
	if ( arp_timer < 10 )                 // Check timer
  {
    return FALSE ;                      // Not expired
  }
  arp_timer = 0 ;                       // Expired, reset
  return TRUE ;
}


//******************************************************************************
//                    D E L A Y _ T I M E R 0                                  *
//******************************************************************************
// Get the value of delay timer 0.                                             *
//******************************************************************************
uint16_t delay_timer0()
{
  return timer0 ;
}


//******************************************************************************
//                              S E C O N D S                                  *
//******************************************************************************
// Get the value of seconds counter                                            *
//******************************************************************************
uint32_t seconds()
{
  return second_counter ;
}


//******************************************************************************
//                       TIM2_UPD_OVF_BRK_IRQHandler                           *
//******************************************************************************
// Interrupt handler for TIM2 interrupts, every 1 msec.                        *
//******************************************************************************
INTERRUPT_HANDLER ( TIM2_UPD_OVF_BRK_IRQHandler, 13 )
{
  static uint16_t lseccount = 0 ;                   // Counter for one second

  if ( msecdelay )                                  // Delaycounter active?
  {
    msecdelay-- ;                                   // Yes, decrement it
  }
  timer0++ ;                                        // Increment free running timer
  if ( periodic_timer != 0xFF )                     // Inc periodic timer if necessary
  {
    periodic_timer++ ;
  }
  if ( ++lseccount == 1000 )                        // Second passed?
  {
    second_counter++ ;                              // Yes, increment second counter
    lseccount = 0 ;                                 // Start new second
    if ( arp_timer != 0xFF )                        // Inc arp timer if necessary
    {
      arp_timer++ ;                                 // Count seconds
    }
    if ( pwok )                                     // Password okay timer still running?
    {
      pwok-- ;                                      // Yes, decrement until zero reached
    }
  }
  TIM2->SR1 &= ~TIM2_SR1_UIF ;                      // Clear interrupt flag
}


//******************************************************************************
//                     F L A S H _ W R I T E _ E N A B L E                     *
//******************************************************************************
// Enable/disable flash memory for write.                                      *
//******************************************************************************
void flash_write_enable ( uint8_t enable )
{
  if ( enable )
  {
    FLASH->PUKR = FLASH_RASS_KEY1 ;                      // Unlock flash
    FLASH->PUKR = FLASH_RASS_KEY2 ;
    while ( ! ( FLASH->IAPSR & ( FLASH_IAPSR_PUL ) ) ) ; // Wait for unlock
  }
  else
  {
    FLASH->IAPSR &= ~FLASH_IAPSR_PUL ;                   // Lock flash
  }
}


//******************************************************************************
//                    E E P R O M _ W R I T E _ E N A B L E                    *
//******************************************************************************
// Enable/disable EEPROM memory for write.                                     *
//******************************************************************************
void eeprom_write_enable ( uint8_t enable )
{
  if ( enable )
  {
    FLASH->DUKR = EEPROM_RASS_KEY1 ;                 // Unlock EEPROM
    FLASH->DUKR = EEPROM_RASS_KEY2 ;
  }
  else
  {
    FLASH->IAPSR &= ~FLASH_IAPSR_DUL ;               // Lock EEPROM
  }
}


//******************************************************************************
//                           S E R I A L _ I N I T                             *
//******************************************************************************
// Initialize Uart2 for serial I/O.                                            *
// Pins:                                                                       *
// UART2_RX PD6                                                                *
// UART2_TX PD5                                                                *
// Main clock seems to run on 17.5 MHz.                                        *
// Divider for Baudrate is 17.5 MHz / 115200 Hz = 148 = 0x94                   *
//******************************************************************************
void serial_init()
{
  UART2->BRR1 = 0x09 ;                          // Set baudrate
  UART2->BRR2 = 0x04 ;
  UART2->CR2  = UART2_CR2_TEN | UART2_CR2_REN ; // Transmit and receive enable
}  


//******************************************************************************
//                           S E R I A L _ W R I T E                           *
//******************************************************************************
// Send a single character to the serial output (pin 11).                      *
//******************************************************************************
void serial_write ( char data )
{
  while ( ! ( UART2->SR & UART2_SR_TC ) )     // Wait for transmission complete
  {
    IWDG_Reload() ;                           // Take care of watchdog
  }
  UART2->DR = data ;                          // Send one character
}

//******************************************************************************
//                      S E R I A L _ W R I T E _ S T R                        *
//******************************************************************************
// Send a string to the serial output (pin 11).                                *
//******************************************************************************
void serial_write_str ( const char* data )
{
  while ( *data )                             // String end reched?
  {
    serial_write ( *data++ ) ;                // No, send data
  }
}


//******************************************************************************
//                      S E R I A L _ W R I T E _ S T R _ L F                  *
//******************************************************************************
// Send a string to the serial output followed by a linefeed.                  *
//******************************************************************************
void serial_write_str_lf ( const char* data )
{
  serial_write_str ( data ) ;                 // Send the string
  serial_write ( '\n' ) ;                     // And send newline
}


//******************************************************************************
//                                   H E X 2 A                                 *
//******************************************************************************
// Convert a nibble to the hexadecimal format.                                 *
//******************************************************************************
char hex2a ( uint8_t nib )
{
  nib &= 0x0F ;                               // Isolate nibble
  if ( nib > 9 )
  {
    return ( 'A' + nib - 10 ) ;               // A..F
  }
  return nib + '0' ;                          // 0..9
}


//******************************************************************************
//                      S E R I A L _ W R I T E _ H E X                        *
//******************************************************************************
// Send a string followed by a hexadecimal value to serial output.             *
//******************************************************************************
void serial_write_hex ( const char* data, uint16_t h )
{
  uint8_t i ;

  serial_write_str ( data ) ;                 // Send the string
  for ( uint8_t i = 0 ; i < 4 ; i++ )         // Loop for 4 nibbles
  {
    serial_write ( hex2a ( h >> 12 ) ) ;
    h = h << 4 ;                              // Next nibble
  }
  serial_write ( '\n' ) ;                     // And send newline
}

//******************************************************************************
//                      S E R I A L _ D U M P _ H E X                          *
//******************************************************************************
// print buffer as hexadecimal values to serial output.                        *
//******************************************************************************
void serial_dump_hex ( const uint8_t* data, uint16_t len )
{
  uint8_t count = 0 ;

  while ( len-- )
  {
    serial_write ( hex2a ( *data >> 4 ) ) ;
    serial_write ( hex2a ( *data++ ) ) ;
    serial_write ( ' ' ) ;                      // Send space
    if ( ( ( ++count & 0x0F ) == 0 ) ||         // Every 16th byte
           ( len == 0 ) )                       // Or last byte
    {
      serial_write ( '\n' ) ;                   // Send newline
    }
  }
}
