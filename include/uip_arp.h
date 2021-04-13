/**
 * Macros and definitions for the ARP module.
 * \author Adam Dunkels <adam@dunkels.com>
 */
  

/*
 * Copyright (c) 2001-2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: uip_arp.h,v 1.5 2006/06/11 21:46:39 adam Exp $
 *
 */
 
/* Modifications 2020 Michael Nielson
 * Adapted for STM8S005 processor, ENC28J60 Ethernet Controller,
 * Web_Relay_Con V2.0 HW-584, and compilation with Cosmic tool set.
 * Author: Michael Nielson
 * Email: nielsonm.projects@gmail.com
 *
 * This declaration applies to modifications made by Michael Nielson
 * and in no way changes the Adam Dunkels declaration above.
 
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



#ifndef __UIP_ARP_H__
#define __UIP_ARP_H__

#include "uip.h"


extern struct uip_eth_addr uip_ethaddr;

/**
 * The Ethernet header.
 */
struct uip_eth_hdr
{
  struct uip_eth_addr dest ;        // 6 byte mac address
  struct uip_eth_addr src ;         // 6 byte mac address
  uint16_t            type ;
};

struct arp_hdr
{
  struct uip_eth_hdr  ethhdr ;       // 6 bytes destination, usually FF:FF:FF:FF:FF:FF plus
                                     // 6 bytes source, MAC address of caller plus
                                     // Type usually 08 06
  uint16_t            hwtype ;       // Hardware type usually 00 01
  uint16_t            protocol ;     // Protocol type, usually 08 00 
  uint8_t             hwlen ;        // Hardware address length, usually 06
  uint8_t             protolen ;     // Protocol address length usually 04
  uint16_t            opcode ;       // Opcode, 00 01 for request
  struct uip_eth_addr shwaddr ;      // Sender MAC address, 6 bytes
  uint8_t             sipaddr[4] ;   // Sender IP address, 4 bytes
  struct uip_eth_addr dhwaddr ;      // Destination MAC address, usually 00:00:00:00:00:00 
  uint8_t             dipaddr[4] ;   // Destination IP address, for example 192.168.2.88
} ;

struct ethip_hdr
{
  struct uip_eth_hdr ethhdr ;        // 6 bytes destination, usually FF:FF:FF:FF:FF:FF plus
                                     // 6 bytes source, MAC address of caller plus
                                     // Type usually 08 06
  uint8_t    vhl ;                   // Variable header length
  uint8_t    tos ;
  uint8_t    len[2] ;
  uint8_t    ipid[2] ;
  uint8_t    ipoffset[2] ;
  uint8_t    ttl ;
  uint8_t    proto ;
  uint16_t   ipchksum ;
  uint8_t    srcipaddr[4] ;         // Source IP address
  int8_t     destipaddr[4] ;        // Destination IP address
} ;


#define UIP_ETHTYPE_ARP 0x0806
#define UIP_ETHTYPE_IP  0x0800
#define UIP_ETHTYPE_IP6 0x86dd


/* The uip_arp_init() function must be called before any of the other
   ARP functions. */
void uip_arp_init (void) ;

/* The uip_arp_ipin() function should be called whenever an IP packet
   arrives from the Ethernet. This function refreshes the ARP table or
   inserts a new mapping if none exists. The function assumes that an
   IP packet with an Ethernet header is present in the uip_buf buffer
   and that the length of the packet is in the uip_len variable. */
/*void uip_arp_ipin(void);*/
#define uip_arp_ipin()

/* The uip_arp_arpin() should be called when an ARP packet is received
   by the Ethernet driver. This function also assumes that the
   Ethernet frame is present in the uip_buf buffer. When the
   uip_arp_arpin() function returns, the contents of the uip_buf
   buffer should be sent out on the Ethernet if the uip_len variable
   is > 0. */
void uip_arp_arpin(void) ;

/* The uip_arp_out() function should be called when an IP packet
   should be sent out on the Ethernet. This function creates an
   Ethernet header before the IP header in the uip_buf buffer. The
   Ethernet header will have the correct Ethernet MAC destination
   address filled in if an ARP table entry for the destination IP
   address (or the IP address of the default router) is present. If no
   such table entry is found, the IP packet is overwritten with an ARP
   request and we rely on TCP to retransmit the packet that was
   overwritten. In any case, the uip_len variable holds the length of
   the Ethernet frame that should be transmitted. */
void uip_arp_out(void);

/* The uip_arp_timer() function should be called every ten seconds. It
   is responsible for flushing old entries in the ARP table. */
void uip_arp_timer(void);

#endif /* __UIP_ARP_H__ */
