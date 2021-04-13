/*
 * This file implements all necessary Callback functions for
 * the uip Stack to route data properly to the right application.
 * 
 * Author: Simon Kueppers
 * Email: simon.kueppers@web.de
 * Homepage: http://klinkerstein.m-faq.de
 * 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 Copyright 2008 Simon Kueppers
 *
 */
 
/* Modifications 2020 Michael Nielson
 * Adapted for STM8S005 processor, ENC28J60 Ethernet Controller,
 * Web_Relay_Con V2.0 HW-584, and compilation with Cosmic tool set.
 * Author: Michael Nielson
 * Email: nielsonm.projects@gmail.com
 *
 * This declaration applies to modifications made by Michael Nielson
 * and in no way changes the Simon Kueppers declaration above.
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 See GNU General Public License at <http://www.gnu.org/licenses/>.
 
 Copyright 2020 Michael Nielson
*/

#include "uip_tcpapphub.h"
#include "uip.h"
#include "gpio.h"
#include <string.h>

extern uint16_t Port_Httpd ;

//******************************************************************************
//                    U I P _ T C P A P P H U B C A L L                        *
//******************************************************************************
// We get here via UIP_APPCALL in the uip.c code.                              *
//******************************************************************************
void uip_TcpAppHubCall(void)
// We get here via UIP_APPCALL in the uip.c code
{
  if ( uip_conn->lport == htons ( Port_Httpd ) )          // HTTP request?
  {
    if ( uip_datalen() )
    {
      //serial_dump_hex ( uip_appdata, uip_datalen() ) ;
      //uip_appdata[uip_datalen()] = '\0' ;
      //serial_write_str ( uip_appdata ) ;
    }
    HttpDCall ( uip_appdata, uip_datalen(),
                &uip_conn->appstate.HttpDSocket ) ;
  }
}
