//***************************************************************************************************
//                                           H T T P D . H                                          *
//***************************************************************************************************
// Header file for httpd.c.                                                                         *
//***************************************************************************************************
#ifndef HTTP_H_
#define HTTP_H_

#include <stdint.h>

#define PORT_HTTPD	80

// Struct for network settings and whitelist firewall.
struct netdata_t                                      // For all etwork related data
{
  uint8_t  ip[4] ;                                    // IP address of this host
  uint8_t  mask[4] ;                                  // Mask or IP address
  uint8_t  gw[4] ;                                    // Gateway for this network
  uint8_t  whitelist[8][5] ;                          // Whitelist for access (CIDR, like 192.168.2.128/25)
  char     valpat[12+1] ;                             // Pattern to check valid EEPROM
} ;

enum pageid_t { INDX, STYL, FAVI, CONF,       // IDs for the pages in use
                OUTL, NETW, LOGI, GCNF,
                PCNF, GOUT, GNET, PNET,
                GSRV, GSWT, GMEM, ABUT,
                NFND } ;                      // End of table, not found

struct htmlpg_t                               // For table with HTML pages
{
  const enum pageid_t pgid ;                  // Page ID
  const char*         name ;                  // Name of the page, like "index.html"
  const char*         ctype ;                 // Content type, like "text/html"
  const char*         page ;                  // Address of the page in memory
  const uint8_t       pwrq ;                  // Password required for this page, or admin-only
  const uint16_t      len ;                   // Length of the page in bytes
} ;

struct tHttpD                                 // Variables for one http connection
{
  uint8_t         nState ;                    // State oh handing http data
  const uint8_t*  pData ;                     // Pointer to data to send
  uint16_t        nDataLeft ;                 // Number of bytes to send
  uint16_t        nPrevBytes ;                // Number of bytes for retransmit
  uint8_t*        pPostdata ;                 // Destination of POST data
  uint16_t        nPostBytes ;                // Expected length of POST data
  uint8_t         pginx ;                     // Index in pagetab
  uint8_t         xsok ;                      // Access is okay
} ;

void HttpDInit() ;                            // Initialize HTTP handling
void HttpDCall (	uint8_t* pBuffer,           // Called for every HTTP packet
                  uint16_t nBytes,
                  struct tHttpD* pSocket ) ;
void setNetwork() ;                           // Set network parameters from EEPROM

extern struct netdata_t*   netdata ;

#endif /*HTTP_H_*/
