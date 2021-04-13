//***************************************************************************************************
//                                 A P C S W I T C H                                                *
//***************************************************************************************************
// Main program of modified APC PDU switch with HW-584 ethernet board                               *
//***************************************************************************************************
#include <stm8s.h>
#include <string.h>
#include "stm8s_it.h"
#include "gpio.h"
#include "Spi.h"
#include "Enc28j60.h"
#include "uip.h"
#include "uip_arp.h"

// Global data

const char code_revision[]  = "2021-04-13 10:47 GMT" ;    // Version date and time

const uint16_t Port_Httpd = 80 ;                          // HTTP port number
const uint8_t  OUI[3] = { 0xA4, 0x4A, 0xD3 } ;            // ST organizationally unique identifier
                                                          // Will be first 3 bytes of MAC addess
int8_t        switch_req = -1 ;                           // Channel of outlet to switch (0..7)
                                                          // -1 is inactive

//***************************************************************************************************
//                                 F I R E W A L L _ O K                                            *
//***************************************************************************************************
// Check if source IP is allowed to access this device.                                             *
//***************************************************************************************************
uint8_t firewall_ok ( const uint8_t* srcipaddr )
{
  uint8_t  i ;                                              // Index in IP whitelist
  uint8_t  j ;                                              // Index in IP address (0..3)
  uint8_t  n ;                                              // Number of bits according CIDR notation
  uint32_t mask32 ;                                         // Mask as a 32 bits number
  uint8_t  mask[4] ;                                        // Mask as 4 bytes
  uint8_t  found ;                                          // IP found in whitelist if TRUE

  if ( srcipaddr[0] == 0 )                                  // 0.x.x.x is reserved
  {
    return TRUE ;
  }
  for ( i = 0 ; i < 8 ; i++ )                               // Search whitelist
  {
    IWDG_Reload() ;                                         // Feed the watchdog
    n = netdata->whitelist[i][4] ;                          // Get number of bits to check
    if ( n == 0 )                                           // Skip whitelist entries with zero mask
    {
      continue ;                                            // Skip this entry
    }
    mask32 = 0xFFFFFFFF << ( 32 - n ) ;                     // Create mask of 32 bits
    mask[0] = mask32 >> 24 & 0xFF ;
    mask[1] = mask32 >> 16 & 0xFF ;
    mask[2] = mask32 >>  8 & 0xFF ;
    mask[3] = mask32       & 0xFF ;
    found = TRUE ;                                          // Assume IP is in whitelist
    for ( j = 0 ; j < 4 ; j++ )
    {
      if ( ( srcipaddr[j] ^                                 // Any bit incorrect?
             netdata->whitelist[i][j] ) &
           mask[j] )
      {
        found = FALSE ;                                     // Incorrect bit in IP found
        break ;                                             // No need to searh further  
      }
    }
    if ( found )                                            // IP in whitelist?
    {
      return TRUE ;                                         // Yes, return TRUE
    }
  }
  serial_write_str_lf ( "Firewall blocked package" ) ;
  return FALSE ;                                            // IP not in whitelist
}


//***************************************************************************************************
//                                           M A I N                                                *
//***************************************************************************************************
// Main program.                                                                                    *
//***************************************************************************************************
void main()
{
  uint16_t          type ;                              // Packet type
  uint32_t          sw_timer = 0 ;                      // Timer for output switch
  int8_t            local_switch_req = -1 ;             // Channel to switch
  struct ethip_hdr* ip_hdr ;                            // Pointer to IP header of package
  uint8_t           chn ;                               // Relais output channel 0..7

  clk_init() ;
  gpio_init() ;                                         // Init GPIO
  serial_init() ;                                       // Initialize serial I/O
  serial_write_str ( "\nAPCswitch version " ) ;         // Show activity
  serial_write_str ( code_revision ) ;                  // Version
  serial_write_str_lf ( " started" ) ;
  enableInterrupts() ;
  if ( uip_ethaddr.addr[0] != OUI[0] )                  // MAC address filled?
  {
    memcpy ( uip_ethaddr.addr, OUI, 3 ) ;               // No, fill 3 bytes with OUI
    memcpy ( uip_ethaddr.addr + 3,                      // and 3 bytes with
             (uint8_t*)0x48CD, 3 ) ;                    //  STM8 unique ID
  }
  delay_ms_wd ( 1200 ) ;                                // Wait for PlatformI/O switch to monitor
  setNetwork() ;                                        // Set network parameters from EEPROM
  serial_write_str ( "IP  address " ) ;                 // Show IP address
  serial_dump_hex ( uip_hostaddr, 4 ) ;
  serial_write_str ( "IP  mask    " ) ;                 // Show mask
  serial_dump_hex ( uip_netmask, 4 ) ;
  serial_write_str ( "Gateway     " ) ;                 // Show gateway
  serial_dump_hex ( uip_draddr, 4 ) ;
  IWDG_init() ;                                         // Initialize the hardware watchdog
  spi_init() ;                                          // Initialize the SPI interface
  Enc28j60Init() ;                                      // Initialize the ENC28J60
  uip_arp_init() ;                                      // Initialize the ARP module
  uip_init() ;                                          // Initialize uIP Web Server
  HttpDInit() ;                                         // Init HTTP server
  serial_write_str ( "MAC address " ) ;                 // Show MAC address
  serial_dump_hex ( uip_ethaddr.addr, 6 ) ;
  for ( chn = 0 ; chn < 8 ; chn++ )                     // All relais output HIGH (open drain)
  {
    outlet_set ( chn, TRUE ) ;                          // Set one output to HIGH
  }
  ENABLEOUTPUT ;                                        // Enable outputs (GPIO G1 HIGH)
  while ( TRUE )
  {
    IWDG_Reload() ;                                     // Feed the watchdog
    uip_len = Enc28j60Receive ( uip_buf ) ;             // Check for incoming packets
    if ( uip_len )
    {
      LEDset ( TRUE ) ;                                 // Show soma activity
      ip_hdr = ((struct ethip_hdr*)uip_buf) ;           // Yes, make access pointer
      type = ip_hdr->ethhdr.type ;                      // Get packet type
      //serial_dump_hex ( uip_buf, uip_len ) ;
      if ( type == htons ( UIP_ETHTYPE_IP ) )           // IP-packet?
      {
        if ( firewall_ok ( ip_hdr->srcipaddr ) )
        {
          uip_input() ;                                 // Process a received packet.
          // If the above process resulted in data that should be sent out on
          // the network the global variable uip_len will have been set to a
          // value > 0.
          if ( uip_len )                                // Reply expected?
          {
            IWDG_Reload() ;                             // Feed the watchdog
            uip_arp_out() ;                             // Yes, create header
            Enc28j60Send ( uip_buf, uip_len ) ;         // and send reply
          }
        }
      }
      else if ( type == htons ( UIP_ETHTYPE_ARP ) )
      {
        uip_arp_arpin() ;                               // Process arp in message
        if ( uip_len )                                  // Reply expected?
        {
          Enc28j60Send ( uip_buf, uip_len ) ;           // Yes, send reply
        }
      }
    }
    if ( periodic_timer_expired() )
    {
      // The periodic timer expires every 20ms.
      for ( uint8_t i = 0 ; i < UIP_CONNS ; i++ )
      {
	      uip_periodic ( i ) ;
        // uip_periodic() calls uip_process(UIP_TIMER) for each connection.
        // uip_process(UIP_TIMER) will check the HTTP connections
        // for any unserviced outbound traffic. HTTP can have pending
        // transmissions because the web pages can be broken into several
        // packets.
        //
        // If uip_periodic() resulted in data that should be sent out on
        // the network the global variable uip_len will have been set to a
        // value > 0.
        //
        if ( uip_len )
        {
          LEDset ( TRUE ) ;                             // Show activity
          uip_arp_out() ;                               // Verifies arp entry in the ARP table and builds
                                                        // the LLH
          Enc28j60Send ( uip_buf, uip_len ) ;
      	}
      }
    }
    if ( arp_timer_expired() )                          // Call the ARP timer function every 10 seconds.
    {
      //serial_write_str_lf ( "Per. timer expired" ) ;
      uip_arp_timer() ;                                 // Clean out old ARP Table entries. Any entry that has
                                                        // exceeded the UIP_ARP_MAXAGE without being accessed
                                                        // is cleared. UIP_ARP_MAXAGE is typically 20 minutes.
    }
    LEDset ( FALSE ) ;                                  // End of activity
    if ( sw_timer )                                     // Switch active?
    {
      if ( seconds() >= sw_timer  )                      // Yes, end of period?
      {
        serial_write_hex ( "Switch on chn ",
                           local_switch_req ) ;
        outlet_set ( local_switch_req, TRUE ) ;         // Yes, switch it on (power on)
        sw_timer = 0 ;                                  // Deactivate timer
      }
    }
    if ( switch_req >= 0 )                              // Request to switch a relais?
    { 
      if ( sw_timer == 0 )                              // Yes, already active? 
      {
        serial_write_hex ( "Switch off chn ",
                           switch_req ) ;
        outlet_set ( switch_req, FALSE ) ;              // Yes, switch it off (power off)
        local_switch_req = switch_req ;                 // No, copy channel nr for on switch
        sw_timer = ( seconds() + 10 ) | 1 ;             // Set timer for 10 seconds, may not be 0
        switch_req = -1 ;                               // Request seen
      }
    }
    if ( RESETBUTTON || input_read ( 0 ) )              // Check "reset" button and input 0
    {
      serial_write_str_lf ( "Reset button pushed" ) ;
    }
    while ( RESETBUTTON || input_read ( 0 ) ) ;         // Long push will cause WD reset
  }                                                     // End of while loop
}