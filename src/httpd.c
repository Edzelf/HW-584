//***************************************************************************************************
//                                           H T T P D . C                                          *
//***************************************************************************************************
// Handling of HTTP requests.                                                                       *
//***************************************************************************************************
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "httpd.h"
#include "uip.h"
#include "gpio.h"

#define STATE_CONNECTED    0                          // Client has just connected
#define STATE_RECEIVING    1                          // Receiving more data
#define STATE_SENDHEADER   2                          // Need to send the HTTP header
#define STATE_SENDDATA     3                          // Need to send the data

#define APCUSERNUM        8                           // Max 8 users wi8th passwords allowed
#define APCOUTLETNUM      8                           // 8 APC outlets
#define RWFLASH      0xFF00                           // Start of flash area reserved for apcdata
#define RWEEPROM     0x4000                           // Start of EEPROM area reserved for netdata

struct pwentry_t                                      // For user entries in flash
{
  char     username[8] ;                              // User name
  char     password[8] ;                              // Password for this user
  char     mask[8] ;                                  // Allowed outlets, example: '1__456__"
} ;

// Struct for the 8 possible outlets in flash
struct outlet_t                                       // For outlet entries in flash
{
  char     server_id[8] ;                             // Server ID, like "SH-04"
} ;

// Struct for apcdata.    sizeof(struct apcdata_t) must be less than 256 bytes!!!
struct apcdata_t                                      // For all APC related data
{
  struct  pwentry_t   userinf[APCUSERNUM] ;           // Info in (RW)FLASH for all users
  struct  outlet_t    outletinf[APCOUTLETNUM] ;       // Info in (RW)FLASH for all outlets
} ;

#include "../data/index.html.inc"
#include "../data/config.html.inc"
#include "../data/outlets.html.inc"
#include "../data/network.html.inc"
#include "../data/style.css.inc"
#include "../data/favicon.ico.inc"
#include "../data/about.html.inc"

const char ct_html[]  = "text/html" ;                    // MIME (contents) type for HTML
const char ct_css[]   = "text/css" ;                     // MIME (contents) type for CSS
const char ct_plain[] = "text/plain" ;                   // MIME (contents) type for plain text
const char ct_ico[]   = "image/x-icon" ;                 // MIME (contents) type for icon
const char ct_bin[]   = "application/octet-stream" ;     // MIME (contents) type for binary data
 
const char sorry_html[]  = "Sorry" ;                     // "Not found" page or password error
const char pwok_html[]   = "Password okay. "             // Reply if password is okay
                           "Access for 5 minutes." ;
const char csok_html[]   = "Config saved" ;              // Reply if configuration has been saved
const char swok_html[]   = "Switch activated" ;          // Reply if switch activated
const char cl[]          = "Content-Length" ;            // Handy constant
const struct netdata_t defnet =
          { { 192, 168,   2,  88 },                      // Default IP
            { 255, 255, 255,   0 },                      // Default mask
            { 192, 168,   2, 254 },                      // Default gateway
            { { 192, 168,   2,   0, 24 },                // Whitelist (CIDR format), entry 1/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 2/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 3/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 4/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 5/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 6/8
              {   0,   0,   0,   0,  0 },                // Whitelist (CIDR format), entry 7/8
              {   0,   0,   0,   0,  0 }                 // Whitelist (CIDR format), entry 8/8
            },
            { "EEPROM Valid" }
          } ;
const char tokdel[] = "?=: \r\n" ;                       // Delimeters for tokstr.  Note that "&" does not work

#define AONLY ( TRUE + 1 )                               // Code for "admin only"in pwrq field
const struct htmlpg_t pagetab[] =
{ // The default page should be in the first entry
  //pgid  name            ctype     page          pwrq   len
  { INDX, "index.html",   ct_html,  index_html,   FALSE, sizeof(index_html) - 1   }, // The home page
  { STYL, "style.css",    ct_css,   style_css,    FALSE, sizeof(style_css) - 1    }, // The style sheet
  { FAVI, "favicon.ico",  ct_ico,   favicon_ico,  FALSE, sizeof(favicon_ico)      }, // The icon
  { CONF, "config.html",  ct_html,  config_html,  AONLY, sizeof(config_html) - 1  }, // The config page
  { OUTL, "outlets.html", ct_html,  outlets_html, TRUE,  sizeof(outlets_html) - 1 }, // The outlets page
  { NETW, "network.html", ct_html,  network_html, AONLY, sizeof(network_html) - 1 }, // The network page
  { ABUT, "about.html",   ct_html,  about_html,   FALSE, sizeof(about_html) - 1   }, // The about page
  { LOGI, "login",        ct_plain, pwok_html,    TRUE,  sizeof(pwok_html) - 1    }, // The login request
  { GCNF, "get_config",   ct_plain, NULL,         AONLY, sizeof(struct apcdata_t) }, // The get config request
  { PCNF, "put_config",   ct_plain, NULL,         AONLY, sizeof(struct apcdata_t) }, // The save config request
  { GNET, "get_network",  ct_bin,   NULL,         AONLY, sizeof(struct netdata_t) }, // The get network request
  { PNET, "put_network",  ct_bin,   NULL,         AONLY, sizeof(struct netdata_t) }, // The save network request
  { GSRV, "get_svrids",   ct_plain, NULL,         TRUE,  APCOUTLETNUM * 8         }, // The get outlets request
  { GOUT, "get_outlets",  ct_plain, NULL,         TRUE,  APCOUTLETNUM             }, // The get server id request
  { GSWT, "get_switch",   ct_plain, swok_html,    TRUE,  sizeof(swok_html) - 1    }, // The activate switch request
  { NFND, NULL,           ct_plain, sorry_html,   FALSE, sizeof(sorry_html) - 1,  }  // End of table, not found page
} ;

// Forward declarations
void        chkpage ( struct tHttpD* pSocket ) ;
const char* getparvalue ( const char* parname ) ;


struct apcdata_t*   apcdata = (struct apcdata_t*)RWFLASH ;   // Points to data in RW flash
struct netdata_t*   netdata = (struct netdata_t*)RWEEPROM ;  // Points to data in EEPROM
extern uint8_t      switch_req ;                             // Channel of outlet (0..7) to be switched
uint8_t             userInx ;                                // User index in apcdata, 0 is "admin"

//***************************************************************************************************
//                                S E T N E T W O R K                                               *
//***************************************************************************************************
// Set network parameters from the netdata struct in EEPROM.                                        *
// The 8 whitelisted area's are kept in EEPROM.                                                     *
//***************************************************************************************************
void setNetwork()
{
  const char* defupw = "admin   " ;                          // Default username and password

  if ( strcmp ( netdata->valpat, defnet.valpat ) != 0 )      // Network data in EEPROM?
  {
    eeprom_write_enable ( TRUE ) ;                           // No, enable EEPROM write
    memcpy ( netdata, &defnet, sizeof(defnet) ) ;            // and use default
    eeprom_write_enable ( FALSE ) ;                          // Disable EEPROM write
    flash_write_enable ( TRUE ) ;                            // Enable flash
    memset ( apcdata, ' ', sizeof(struct apcdata_t) ) ;      // Erase flash
    strncpy ( apcdata->userinf[0].username, defupw, 8 ) ;    // Set default username
    strncpy ( apcdata->userinf[0].password, defupw, 8 ) ;    // Set default password
    flash_write_enable ( FALSE ) ;                           // Disable flash
  }
  memcpy ( uip_hostaddr, netdata->ip,   4 ) ;                // Network data is in EEPROM, use it
  memcpy ( uip_netmask,  netdata->mask, 4 ) ;
  memcpy ( uip_draddr,   netdata->gw,   4 ) ;
}


//***************************************************************************************************
//                                   S T R I E Q                                                    *
//***************************************************************************************************
// Check for equality of two strings.  Case insensitive.  It is a replacement for strcasestr,       *
// which is not available in the STM8 libraries.                                                    *
//***************************************************************************************************
uint8_t strieq ( const char* s1, const char* s2 )
{
  uint8_t l1 = strlen ( s1 ) ;
  uint8_t l2 = strlen ( s2 ) ;

  if ( strlen ( s1 ) != strlen ( s2 ) )
  {
    return FALSE ;
  }
  while ( *s1  )
  {
    if ( toupper ( *s1++ ) != toupper ( *s2++ ) )
    {
      return FALSE ;
    }
  }
  return TRUE ;
}


//***************************************************************************************************
//                                     C O P Y S T R I N G P                                        *
//***************************************************************************************************
// Copy a null terminated string to the output buffer.                                              *
//***************************************************************************************************
static uint16_t CopyStringP ( uint8_t**   ppBuffer,
                              const char* pString )
{
  uint16_t nBytes = 0 ;
  char     Character ;

  while ( ( Character = pString[0] ) != '\0' )
  {
    **ppBuffer = Character ;
    *ppBuffer = *ppBuffer + 1 ;
    pString = pString + 1 ;
    nBytes++ ;
  }
  return nBytes ;
}


//****************************************************************************************************
//                                       C O P Y V A L U E                                           *
//****************************************************************************************************
// Add the ascii representation of a number (max 10000) to a string.                                 *
//****************************************************************************************************
uint16_t CopyValue (	uint8_t** ppBuffer, uint32_t nValue )
{
  int16_t nDec = 10000 ;
  char    cNumber ;
  uint8_t nBytes = 0 ;

  while ( nDec )
  {
    for ( cNumber = '0'; cNumber < '9' ; cNumber++ )
    {
      int32_t nTmp = nValue - nDec ;
      if ( nTmp < 0 )
      {
        break ;
      }
      nValue = nTmp ;
    }
    **ppBuffer = cNumber ;
    *ppBuffer = *ppBuffer + 1 ;
    nBytes++ ;
    nDec /= 10 ;
  }
  return nBytes ;
}


//****************************************************************************************************
//                                 C O P Y H T T P H E A D E R                                       *
//****************************************************************************************************
// Copy the HTTP header to the buffer.                                                               *
//****************************************************************************************************
static uint16_t CopyHttpHeader (	uint8_t*    pBuffer,
                                  uint32_t    nDataLen,
                                  const char* mimetype )
{
  return CopyStringP ( &pBuffer, "HTTP/1.1 200 OK\r\n"
                                  "Content-Length: ")  +
         CopyValue   ( &pBuffer, nDataLen ) +
         CopyStringP ( &pBuffer, "\r\n"
                                  "Content-Type: " ) +
         CopyStringP ( &pBuffer,  mimetype ) +
         CopyStringP ( &pBuffer,  "\r\n"
                                  "Connection: close\r\n\r\n" ) ;
}


//****************************************************************************************************
//                                   C O P Y H T T P D A T A                                         *
//****************************************************************************************************
// Copy payload to the buffer.                                                                       *
//****************************************************************************************************
uint16_t CopyHttpData (	uint8_t*     pBuffer,
                        const char** ppData,
                        uint16_t*    pDataLeft,
                        uint16_t     nMaxBytes )
{
  uint16_t nBytes = 0 ;
  uint8_t nByte ;

  while ( ( nMaxBytes-- ) && *pDataLeft )
  {
    nByte = **ppData ;
    *pBuffer = nByte ;
    *ppData = *ppData + 1 ;
    *pDataLeft = *pDataLeft - 1 ;
    pBuffer++ ;
    nBytes++ ;
  }
  return nBytes ;
}


//****************************************************************************************************
//                                         H T T P D I N I T                                         *
//****************************************************************************************************
// Initialize the HTTP handling.                                                                     *
//****************************************************************************************************
void HttpDInit()
{
  uip_listen ( HTONS ( PORT_HTTPD ) ) ;    //Start listening on our port
}


//***************************************************************************************************
//                                           U P C M P                                               *
//***************************************************************************************************
// Check if username or password matches.                                                           *
// Note that the name and password in apcdata is not terminated by a '\0'.                          *
//***************************************************************************************************
uint8_t upcmp ( const char* sb, const char* is )
{
  uint8_t i ;
  uint8_t len_is ;                                      // Length of input name/password
  uint8_t len_sb ;                                      // Length of name/password in apcdata

  len_is = strlen ( is ) ;                              // Get input length
  len_sb = strchr ( sb, ' ' ) - sb ;                    // Get length in table, max 8
  if ( len_sb > 8 )                                     // Cannot be > 8
  {
    len_sb = 8 ;                                        // Limit to 8
  }
  if ( ( len_is != len_sb ) || ( len_is > 8 ) )         // Check for equal and reasonable length
  {
    return FALSE ;                                      // No match
  }
  for ( i = 0 ; i < len_sb ; i++ )                      // Compare strings
  {
    if ( *sb++ != *is++ )                               // Match?
    {
      return FALSE ;                                    // No, unequal
    }
  }
  return TRUE ;                                         // Username/password okay
}


//***************************************************************************************************
//                                    C H K P W                                                     *
//***************************************************************************************************
// Check username and password in the URL.                                                          *
//***************************************************************************************************
uint8_t chkpw()
{
  const char* val ;                                   // Username or password
  int         inx ;                                   // Index in apcdata->userinf
  const char  *up ;                                   // Points to user or password
  uint8_t     res ;                                   // Function result

  val = getparvalue ( "user" ) ;                      // Get name of user in URL
  serial_write_str ( "chkpw-u: " ) ;
  serial_write_str_lf ( val ) ;
  for ( inx = 0 ; inx < APCUSERNUM ; inx++ )          // Search user in apcdata
  {
    up = apcdata->userinf[inx].username ;             // Points to username in apcdata
    if ( upcmp ( up, val ) )                          // User in this entry?
    {
      break ;                                         // No need to continue
    }
  }
  val = getparvalue ( "password" ) ;                  // Yes, get password of user in URL
  serial_write_str ( "chkpw-p: " ) ;
  serial_write_str_lf ( val ) ;
  res = ( inx < APCUSERNUM ) &&                      // Return TRUE if user found
        upcmp ( apcdata->userinf[inx].password,      // and password okay
                val ) ;
  if ( res )                                         // User/Password match?
  {
    pwok = 300 ;                                     // Yes, password okay for 5 minutes
    userInx = inx ;                                  // Make user index global
  }
  else
  {
    serial_write_str_lf ( "Password failure" ) ;     // Show password failure
  }
  return res ;
}


//****************************************************************************************************
//                                         H T T P D C A L L                                         *
//****************************************************************************************************
// Handle a uIP input buffer.                                                                        *
//****************************************************************************************************
void HttpDCall (	uint8_t* pBuffer, uint16_t nBytes, struct tHttpD* pSocket )
{
  uint16_t               n ;                            // Length of data
  const char*            token ;
  uint8_t                pginx ;                        // Index in page table      
  char                   get_post = '\0' ;              // 'G' for GET, 'P' for POST
  uint8_t*               sod ;                          // Start of data (POST)

  pginx = pSocket->pginx ;                              // Assume pginx is known in socket info
  if ( uip_connected() )                                // New connection?
  {
    pSocket->nState = STATE_CONNECTED ;                 // Yes, initialize this connection
    pSocket->nPrevBytes = 0xFFFF ;                      // No previous bytes send
    //serial_write_str_lf ( "New connection " ) ;
  }
  else if ( uip_newdata() || uip_acked() )
  {
    if ( pSocket->nState == STATE_CONNECTED )
    {
      sod = strstr ( pBuffer, "\r\n\r\n" ) ;            // Search end of header
      if ( ( nBytes < 50 ) ||                           // Return if no serious contents
           ( sod == NULL ) )                            // or wrong end of request
      {
        return ;
      }
      sod += 4 ;                                        // Data starts after the "\r\n\r\n"
      token = strtok ( pBuffer, tokdel ) ;              // First token is "GET" or "POST"
      if ( ( strcmp ( token, "GET" ) == 0 ) ||          // Starts with "GET"?
           ( strcmp ( token, "POST" ) == 0 ) )          // Or starts with "POST"?
      {
        get_post = token[0] ;                           // Remember GET or POST function
        token = strtok ( NULL, tokdel ) ;               // Second token is page name
        serial_write_str ( "Request " ) ;               // Show it
        serial_write_str_lf ( token ) ;
        pginx = 0 ;                                     // Assume home page
        if ( strcmp ( token, "/" ) &&                   // Home page?
             !strstr ( token , "index" ) )
        {
          token++ ;                                     // No, forget leading slash
          while ( pagetab[pginx].name )                 // No, search for page name
          {
            if ( strstr ( pagetab[pginx].name,          // Match?
                          token ) )
            {
              if ( pagetab[pginx].pgid == LOGI )        // Yes, working for login on home page?
              {
                chkpw() ;                               // Yes, check password
              }
              if ( ( pwok > 0 ) ||                      // Password okay
                   !pagetab[pginx].pwrq )               // or free access?
              {  
                break ;                                 // Yes, no need to search further
              }
            }
            pginx++ ;                                   // Next entry
          }
        }
        // Now we know what page is serviced. Save some vital page info in the socket
        pSocket->xsok = pwok || !pagetab[pginx].pwrq ;  // Set access okay or not
        if ( ( userInx != 0 ) &&                        // Block if no "admin" user
             ( pagetab[pginx].pwrq == AONLY ) )         // and "admin"-only is requried
        {
          pSocket->xsok = FALSE ;                       // Block access
        }
        pSocket->pginx = pginx ;                        // Set index in page table
        pSocket->pData = pagetab[pginx].page ;          // Set pointer to that page
        pSocket->nDataLeft = pagetab[pginx].len ;       // And the length of the page
        chkpage ( pSocket ) ;                           // Call request specific function
      }
      if ( get_post == 'P' )                            // POST request?
      {
        if ( pagetab[pginx].pgid == PCNF )              // Yes, working for put_config?
        {
          pSocket->pPostdata = (uint8_t*)apcdata ;      // Yes, set data pointer
        }
        else if ( pagetab[pginx].pgid == PNET )         // No, working for put_network?
        {
          pSocket->pPostdata = (uint8_t*)netdata ;      // Yes, set data pointer
        }
        token = getparvalue ( cl ) ;                    // Get content length from header
        pSocket->nPostBytes = atoi ( token ) ;          // Save in socket data
        pSocket->nState = STATE_RECEIVING ;             // Expect more data
        nBytes -= ( sod - pBuffer ) ;                   // Number of DATA bytes left in current buffer
        pBuffer = sod ;                                 // Move to data
      }
      else
      {
        pSocket->nState = STATE_SENDHEADER ;            // End of data, send header
      }
    }
    if ( pSocket->nState == STATE_RECEIVING )           // Receiving data in POST mode
    {
      //serial_write_hex ( "Received POST data, length is ",
      //                   nBytes ) ;
      if ( pagetab[pSocket->pginx].pgid == PCNF )       // Store user configuration?
      {
        flash_write_enable ( TRUE ) ;                   // Enable flash
        memcpy ( pSocket->pPostdata, pBuffer,           // Copy to flash
                 nBytes ) ;
        flash_write_enable ( FALSE ) ;                  // Disable flash
      }
      else if ( pagetab[pSocket->pginx].pgid == PNET )  // Store network configuration
      {
        eeprom_write_enable ( TRUE ) ;                  // Enable EEPROM
        memcpy ( pSocket->pPostdata, pBuffer,           // Copy to EEPROM
                 nBytes ) ;
        eeprom_write_enable ( FALSE ) ;                 // Disable EEPROM
      }
      pSocket->pPostdata += nBytes ;                    // Update destination pointer
      pSocket->nPostBytes -= nBytes ;                   // Rest of data expected
      if ( pSocket->nPostBytes == 0 )                   // Was this the last chunk?
      {
        pSocket->pData = csok_html ;                    // Yes, switch to OK "page"
        pSocket->nDataLeft = sizeof(csok_html) - 1 ;    // Set length
        pSocket->nState = STATE_SENDHEADER ;            // Go send response
      }
    }
    if ( pSocket->nState == STATE_SENDHEADER )
    {
      n = CopyHttpHeader ( uip_appdata,
                           pSocket->nDataLeft,
                           pagetab[pSocket->pginx].ctype ) ;
      // serial_write_hex ( "Send header, len is ", n ) ;
      uip_send ( uip_appdata, n ) ;
      pSocket->nState = STATE_SENDDATA ;
      return ;
    }
    else if ( pSocket->nState == STATE_SENDDATA )
    {
      // We have sent the HTML Header or HTML Data previously.
      // Now we send (further) data depending on the Socket's pData pointer
      // If all data has been sent, we close the connection
      pSocket->nPrevBytes = pSocket->nDataLeft ;
      n = CopyHttpData ( uip_appdata,
                         &pSocket->pData,
                         &pSocket->nDataLeft,
                         uip_mss() ) ;
      pSocket->nPrevBytes -= pSocket->nDataLeft ;
      if ( n == 0 )
      {
        uip_close() ;                                   // No Data has been copied. Close connection
      }
      else
      {
        //serial_write_hex ( "Send data, len is ", n ) ;
        uip_send ( uip_appdata, n ) ;                   // Else send data
      }
    }
  }
  else if ( uip_rexmit() )                              // Retransmit requested?
  {
    serial_write_str_lf ( "Retransmit" ) ;
    if ( pSocket->nPrevBytes == 0xFFFF )                // Yes, header or data?
    {
      n = CopyHttpHeader ( uip_appdata,                 // Header, format it again
                           pSocket->nDataLeft,
                           pagetab[pSocket->pginx].ctype ) ;
      uip_send ( uip_appdata, n ) ;                     // Send header again
    }
    else
    {
      pSocket->pData -= pSocket->nPrevBytes ;           // Retransmit of last data packet
      pSocket->nDataLeft += pSocket->nPrevBytes ;
      pSocket->nPrevBytes = pSocket->nDataLeft ;
      n = CopyHttpData ( uip_appdata,
                         &pSocket->pData,
                         &pSocket->nDataLeft,
                         uip_mss() ) ;
      pSocket->nPrevBytes -= pSocket->nDataLeft ;
      if ( n == 0 )
      {
        //No Data has been copied. Close connection
        uip_close() ;
      }
      else
      {
        //Else send copied data
        uip_send ( uip_appdata, n ) ;
      }
    }
  }
}



//***************************************************************************************************
// Application functions.                                                                           *
//***************************************************************************************************


//***************************************************************************************************
//                             G E T P A R V A L U E                                                *
//***************************************************************************************************
// Returns the value of a parameter in the header.                                                  *
//***************************************************************************************************
const char* getparvalue ( const char* parname )
{
  const char* res = "undef" ;                             // Function result
  const char* token ;                                     // Token from URL
  uint8_t     parfound ;                                  // Parameter name found 

  //serial_write_str ( "Search for parameter " ) ;
  //serial_write_str_lf ( parname ) ;
  token = strtok ( NULL, tokdel ) ;                     // Get first token
  while ( token )                                       // Search
  {
    //serial_write_str ( "Token found " ) ;
    //serial_write_str ( token ) ;
    parfound = strieq ( token, parname ) ;              // True if this is the target parameter
    token = strtok ( NULL, tokdel ) ;                   // Get next token, is value or next token
    if ( parfound )                                     // Target parameter found?
    {
      if ( token )                                      // Valid value?
      {
        //serial_write_str ( ", value is " ) ;
        //serial_write_str ( token ) ;
        res = token ;                                   // Yes, return the value of the target parameter
        break ;                                         // Stop search
      }
    }
  }
  //serial_write_str_lf ( "" ) ;
  return res ;                                          // Return value or "undef"
}

//***************************************************************************************************
//                                    C H K P A G E                                                 *
//***************************************************************************************************
// Check the page for needed checks or contents.                                                    *
//***************************************************************************************************
void chkpage ( struct tHttpD* pSocket )
{
  uint8_t nr ;                                                     // Channel of switchrequested (0..7)

  if ( ! pSocket->xsok )                                           // Access is okay?
  {
    pSocket->pData = sorry_html ;                                  // No, send "sorry"
    pSocket->nDataLeft = sizeof(sorry_html) - 1 ;                  // Adjust length of the page
    return ;
  }
  switch ( pagetab[pSocket->pginx].pgid )
  {
    case GCNF :                                                    // Action on get config request
      pSocket->pData = (const uint8_t*)apcdata ;                   // Set pointer to data to send
      break ;
    case GNET :                                                    // Action on get network config
      pSocket->pData = (const uint8_t*)netdata ;                   // Set pointer to data to send
      break ;
    case GOUT :                                                    // Action on get outlets request
      pSocket->pData = apcdata->userinf[userInx].mask ;            // Set pointer to data to send
      break ;
    case GSRV :                                                    // Action on get server IDs request
      pSocket->pData = (const uint8_t*)apcdata->outletinf ;        // Set pointer to data to send
      break ;
    case GSWT :                                                    // Action on activate outlet request
      nr = atoi ( getparvalue ( "nr" ) ) ;                         // Yes, get outlet channel 0..7 in URL
      // A legal user may try to switch an outlet that is not part of his alowed set of outlets.
      // This requires some hacking in the web interface, but better be safe...
      if ( strchr ( apcdata->userinf[userInx].mask,                // This outlet allowed?
                     nr + '1' ) )
      {
        switch_req = nr ;                                          // Yes, set request for main loop
        //serial_write_hex ( "Outlet (0..7) is ", nr ) ;
      }
      else
      {
        pSocket->pData = sorry_html ;                              // No, send "sorry"
        pSocket->nDataLeft = sizeof(sorry_html) - 1 ;              // Adjust length of the page
      }
      break ;
  }
}
