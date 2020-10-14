// ModbusRtu.cpp

#include "ModbusRtu.h"
#include "application.h"
#include "PinControl.h"

extern PinControl pins;

#define LOGGING // to see what is happening
// create logging buckets for temp
Logger logModbusRtu("lib.rtu");

/* _____PUBLIC FUNCTIONS_____________________________________________________ */

/**
 * @brief
 * Default Constructor for Master through Serial
 *
 * @ingroup setup
 */
Modbus::Modbus() {
  this->u8id = 0;
  this->u8serno = 1;
  this->u16timeOut = 1000;
}

/**
 * @brief
 * Initialize class object.
 *
 * Sets up the serial port using specified baud rate.
 * Call once class has been instantiated, typically within setup().
 *
 * @see http://arduino.cc/en/Serial/Begin#.Uy4CJ6aKlHY
 * @param speed   baud rate, in standard increments (300..115200)
 * @param config  data frame settings (data length, parity and stop bits)
 * @ingroup setup
 */
void Modbus::begin(long u32speed, long configuration) {

  switch( u8serno ) {
  case 1:
    port = &Serial1;
    break;

  case 0:
  default:
    port = &Serial1;
    break;
  }

  // port->begin(u32speed, u8config);
  port->begin(u32speed, configuration);
  rxTxMode(RXEN);

  port->flush();
  u8lastRec = u8BufferSize = 0;
  u16InCnt = u16OutCnt = u16errCnt = 0;
}

/**
 * @brief
 * Method to write a new slave ID address
 *
 * @param 	u8id	new slave address between 1 and 247
 * @ingroup setup
 */
void Modbus::setID( uint8_t u8id) {
  if (( u8id != 0) && (u8id <= 247)) {
    this->u8id = u8id;
  }
}

/**
 * @brief
 * Method to read current slave ID address
 *
 * @return u8id	current slave address between 1 and 247
 * @ingroup setup
 */
uint8_t Modbus::getID() {
  return this->u8id;
}

/**
 * @brief
 * Initialize time-out parameter
 *
 * Call once class has been instantiated, typically within setup().
 * The time-out timer is reset each time that there is a successful communication
 * between Master and Slave. It works for both.
 *
 * @param time-out value (ms)
 * @ingroup setup
 */
void Modbus::setTimeOut( uint16_t u16timeOut) {
  this->u16timeOut = u16timeOut;
}

/**
 * @brief
 * Return communication Watchdog state.
 * It could be usefull to reset outputs if the watchdog is fired.
 *
 * @return TRUE if millis() > u32timeOut
 * @ingroup loop
 */
boolean Modbus::getTimeOutState() {
  return ((unsigned long)(millis() - (unsigned long)u16timeOut) > u32timeOut);
}

/**
 * @brief
 * Get input messages counter value
 * This can be useful to diagnose communication
 *
 * @return input messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getInCnt() {
  return u16InCnt;
}

/**
 * @brief
 * Get transmitted messages counter value
 * This can be useful to diagnose communication
 *
 * @return transmitted messages counter
 * @ingroup buffer
 */
uint16_t Modbus::getOutCnt() {
  return u16OutCnt;
}

/**
 * @brief
 * Get errors counter value
 * This can be useful to diagnose communication
 *
 * @return errors counter
 * @ingroup buffer
 */
uint16_t Modbus::getErrCnt() {
  return u16errCnt;
}

/**
 * Get modbus master state
 *
 * @return = 0 IDLE, = 1 WAITING FOR ANSWER
 * @ingroup buffer
 */
uint8_t Modbus::getState() {
  return u8state;
}

/**
 * Get the last error in the protocol processor
 *
 * @returnreturn   NO_REPLY = 255      Time-out
 * @return   EXC_FUNC_CODE = 1   Function code not available
 * @return   EXC_ADDR_RANGE = 2  Address beyond available space for Modbus registers
 * @return   EXC_REGS_QUANT = 3  Coils or registers number beyond the available space
 * @ingroup buffer
 */
uint8_t Modbus::getLastError() {
  return u8lastError;
}

/**
 * @brief
 * *** Only Modbus Master ***
 * Generate a query to an slave with a modbus_t telegram structure
 * The Master must be in COM_IDLE mode. After it, its state would be COM_WAITING.
 * This method has to be called only in loop() section.
 *
 * @see modbus_t
 * @param modbus_t  modbus telegram structure (id, fct, ...)
 * @ingroup loop
 * @todo finish function 15
 */
int8_t Modbus::query( modbus_t telegram ) {
  // empty rx buffer
  while(port->available()) { port->read(); }
  #ifdef LOGGING
    Serial.print("MODBUS> Query");
    Serial.println();
  #endif
  uint8_t u8regsno, u8bytesno;
  if (u8id!=0) {
    #ifdef LOGGING
      Serial.print("MODBUS> Query Error: No address");
      Serial.println();
    #endif
    return -2;
  }
  if (u8state != COM_IDLE) {
    #ifdef LOGGING
      Serial.print("MODBUS> Query Error: Somebody else transmitting");
      Serial.println();
    #endif
    return -1;
  }

  if ((telegram.u8id==0) || (telegram.u8id>247)) {
    #ifdef LOGGING
      Serial.print("MODBUS> Query Error: Address out of range: ");
      Serial.println(telegram.u8id);
    #endif
    return -3;
  }

  au16regs = telegram.au16reg;

  // telegram header
  au8Buffer[ ID ]         = telegram.u8id;
  au8Buffer[ FUNC ]       = telegram.u8fct;
  au8Buffer[ ADD_HI ]     = highByte(telegram.u16RegAdd );
  au8Buffer[ ADD_LO ]     = lowByte( telegram.u16RegAdd );

  switch( telegram.u8fct ) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUT:
    case MB_FC_READ_REGISTERS:
    case MB_FC_READ_INPUT_REGISTER:
      au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
      au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
      u8BufferSize = 6;
      break;
    case MB_FC_WRITE_COIL:
      au8Buffer[ NB_HI ]      = ((au16regs[0] > 0) ? 0xff : 0);
      au8Buffer[ NB_LO ]      = 0;
      u8BufferSize = 6;
      break;
    case MB_FC_WRITE_REGISTER:
      au8Buffer[ NB_HI ]      = highByte(au16regs[0]);
      au8Buffer[ NB_LO ]      = lowByte(au16regs[0]);
      u8BufferSize = 6;
      break;
    case MB_FC_WRITE_MULTIPLE_COILS: // TODO: implement "sending coils"
      u8regsno = telegram.u16CoilsNo / 16;
      u8bytesno = u8regsno * 2;
      if ((telegram.u16CoilsNo % 16) != 0) {
        u8bytesno++;
        u8regsno++;
      }

      au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
      au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
      au8Buffer[ NB_LO+1 ]    = u8bytesno;
      u8BufferSize = 7;

      u8regsno = u8bytesno = 0; // now auxiliary registers
      for (uint16_t i = 0; i < telegram.u16CoilsNo; i++) {


      }
      break;

    case MB_FC_WRITE_MULTIPLE_REGISTERS:
      au8Buffer[ NB_HI ]      = highByte(telegram.u16CoilsNo );
      au8Buffer[ NB_LO ]      = lowByte( telegram.u16CoilsNo );
      au8Buffer[ NB_LO+1 ]    = (uint8_t) ( telegram.u16CoilsNo * 2 );
      u8BufferSize = 7;

      for (uint16_t i=0; i< telegram.u16CoilsNo; i++) {
        au8Buffer[ u8BufferSize ] = highByte( au16regs[ i ] );
        u8BufferSize++;
        au8Buffer[ u8BufferSize ] = lowByte( au16regs[ i ] );
        u8BufferSize++;
      }
      break;
  }

  #ifdef LOGGING
    Serial.print("MODBUS> Query: tx buffer created");
    Serial.println();
  #endif

  sendTxBuffer();

  #ifdef LOGGING
    Serial.print("MODBUS> Query: tx buffer sent");
    Serial.println();
  #endif
  u8state = COM_WAITING;
  return 0;
}

/**
 * @brief *** Only for Modbus Master ***
 * This method checks if there is any incoming answer if pending.
 * If there is no answer, it would change Master state to COM_IDLE.
 * This method must be called only at loop section.
 * Avoid any delay() function.
 *
 * Any incoming data would be redirected to au16regs pointer,
 * as defined in its modbus_t query telegram.
 *
 * @params	nothing
 * @return errors counter
 * @ingroup loop
 */
int8_t Modbus::poll() {
  // check if there is any incoming frame
  uint8_t u8current = port->available();

  // if there are no bytes available, check if we've gone over our timeout
  if (u8current == 0 && (unsigned long)(millis() - u32timeOut) > (unsigned long)u16timeOut)
  {
    u8state = COM_IDLE;
    u8lastError = NO_REPLY;
    u16errCnt++;
    logModbusRtu.warn("NORPLY");
    return 0;
  }

  // if we have not received any data, don't do anything
  if (u8current == 0)
    return 0;

  // check if we've received any new bytes
  // if we have, allow for some time between receipts
  if (u8current != u8lastRec) {
    u8lastRec = u8current;
    u32time = millis();
    return 0;
  }
  // return if we are still waiting for next byte
  if ((unsigned long)(millis() - u32time) < (unsigned long)T35)
    return 0;

  // transfer Serial buffer frame to auBuffer
  u8lastRec = 0;
  int8_t i8state = getRxBuffer();
  if (
    (i8state < 7 && au8Buffer[ FUNC ] != MB_FC_READ_COILS) ||
    (i8state < 6 && au8Buffer[ FUNC ] == MB_FC_READ_COILS)
  ) {
    u8state = COM_IDLE;
    u16errCnt++;
    logModbusRtu.warn("i8s%i", i8state);
    return i8state;
  }

  // validate message: id, CRC, FCT, exception
  uint8_t u8exception = validateAnswer();
  if (u8exception != 0) {
    u8state = COM_IDLE;
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("u8exception: ");
      Serial.print(u8exception);
      Serial.println();
    #endif
    return u8exception;
  }

  // process answer
  switch( au8Buffer[ FUNC ] ) {
    case MB_FC_READ_COILS:
      get_FC1( );
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_READ_COILS");
        Serial.println();
      #endif
      break;
    case MB_FC_READ_DISCRETE_INPUT:
      // call get_FC1 to transfer the incoming message to au16regs buffer
      get_FC1( );
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_READ_DISCRETE_INPUT");
        Serial.println();
      #endif
      break;
    case MB_FC_READ_INPUT_REGISTER:
      // call get_FC3 to transfer the incoming message to au16regs buffer
      get_FC3( );
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_READ_INPUT_REGISTER");
        Serial.println();
      #endif
      break;
    case MB_FC_READ_REGISTERS :
      // call get_FC3 to transfer the incoming message to au16regs buffer
      get_FC3( );
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_READ_REGISTERS");
        Serial.println();
      #endif
      break;
    case MB_FC_WRITE_COIL:
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_WRITE_COIL");
        Serial.println();
      #endif
      break;
    case MB_FC_WRITE_REGISTER :
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_WRITE_REGISTER");
        Serial.println();
      #endif
      break;
    case MB_FC_WRITE_MULTIPLE_COILS:
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_WRITE_MULTIPLE_COILS");
        Serial.println();
      #endif
      break;
    case MB_FC_WRITE_MULTIPLE_REGISTERS :
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("MB_FC_WRITE_MULTIPLE_REGISTERS");
        Serial.println();
      #endif
      // nothing to do
      break;
    default:
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("nothing to do in the au8 func");
        Serial.println();
      #endif
      break;
  }
  u8state = COM_IDLE;
  #ifdef LOGGING
    Serial.print("MODBUS> ");
    Serial.print("poll OK! Buffer size: ");
    Serial.print(u8BufferSize);
    Serial.println();
  #endif
  return u8BufferSize;
}

/**
 * @brief
 * *** Only for Modbus Slave ***
 * This method checks if there is any incoming query
 * Afterwards, it would shoot a validation routine plus a register query
 * Avoid any delay() function !!!!
 * After a successful frame between the Master and the Slave, the time-out timer is reset.
 *
 * @param *regs  register table for communication exchange
 * @param u16size  size of the register table
 * @return 0 if no query, 1..4 if communication error, >4 if correct query processed
 * @ingroup loop
 */
int8_t Modbus::poll( uint16_t *regs, uint16_t u16size ) {

  au16regs = regs;
  u16regsize = u16size;

  // check if there is any incoming frame
  uint8_t u8current = port->available();
  if (u8current == 0) return 0;

  // check if we've received any new bytes
  // if we have, allow for some time between receipts
  if (u8current != u8lastRec)
  {
    u8lastRec = u8current;
    u32time = millis();
    return 0;
  }
  // return if we are still waiting for next byte
  if ((unsigned long)(millis() - u32time) <  (unsigned long)T35)
    return 0;

  u8lastRec = 0;
  int8_t i8state = getRxBuffer();
  u8lastError = i8state;
  if (i8state < 7) return i8state;

  // check slave id
  if (au8Buffer[ ID ] != u8id) return 0;

  // validate message: CRC, FCT, address and size
  uint8_t u8exception = validateRequest();
  if (u8exception > 0) {
    if (u8exception != NO_REPLY) {
      buildException( u8exception );
      sendTxBuffer();
    }
    u8lastError = u8exception;
    return u8exception;
  }

  u32timeOut = millis();
  u8lastError = 0;

  // process message
  switch( au8Buffer[ FUNC ] ) {
  case MB_FC_READ_COILS:
  case MB_FC_READ_DISCRETE_INPUT:
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_READ_DISCRETE_INPUT");
      Serial.println();
    #endif
    #ifdef DEBUG_LED
      pinMode(D7, OUTPUT);
      digitalWrite(D7, HIGH);
      delay(20);
      digitalWrite(D7, LOW);
    #endif
    return process_FC1( regs, u16size );
    break;
  case MB_FC_READ_INPUT_REGISTER:
  case MB_FC_READ_REGISTERS :
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_READ_REGISTERS");
      Serial.println();
    #endif
    #ifdef DEBUG_LED
      pinMode(D7, OUTPUT);
      digitalWrite(D7, HIGH);
      delay(20);
      digitalWrite(D7, LOW);
    #endif
    return process_FC3( regs, u16size );
    break;
  case MB_FC_WRITE_COIL:
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_WRITE_COIL");
      Serial.println();
    #endif
    return process_FC5( regs, u16size );
    break;
  case MB_FC_WRITE_REGISTER :
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_WRITE_REGISTER");
      Serial.println();
    #endif
    #ifdef DEBUG_LED
      pinMode(D7, OUTPUT);
      digitalWrite(D7, HIGH);
      delay(20);
      digitalWrite(D7, LOW);
    #endif
    return process_FC6( regs, u16size );
    break;
  case MB_FC_WRITE_MULTIPLE_COILS:
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_WRITE_MULTIPLE_COILS");
      Serial.println();
    #endif
    return process_FC15( regs, u16size );
    break;
  case MB_FC_WRITE_MULTIPLE_REGISTERS :
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("MB_FC_WRITE_MULTIPLE_REGISTERS");
      Serial.println();
    #endif
    return process_FC16( regs, u16size );
    break;
  default:
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("nothing to do in the au8 func");
      Serial.println();
    #endif
    break;
  }

  return i8state;
}

/* _____PRIVATE FUNCTIONS_____________________________________________________ */

/**
 * @brief
 * This method moves Serial buffer data to the Modbus au8Buffer.
 *
 * @return buffer size if OK, ERR_BUFF_OVERFLOW if u8BufferSize >= MAX_BUFFER
 * @ingroup buffer
 */
int8_t Modbus::getRxBuffer() {

  boolean bBuffOverflow = false;

  rxTxMode(RXEN);

  u8BufferSize = 0;
  #ifdef LOGGING
    Serial.print("MODBUS> getRxbuffer output: ");
  #endif
  while ( port->available() ) {
    au8Buffer[ u8BufferSize ] = port->read();
    #ifdef LOGGING
      Serial.print(au8Buffer[ u8BufferSize ], HEX);
      Serial.print(" ");
    #endif
    u8BufferSize ++;

    if (u8BufferSize >= MAX_BUFFER) {
      bBuffOverflow = true;
      break;
    };
  }
  #ifdef LOGGING
    Serial.println();
  #endif
  u16InCnt++;

  if (bBuffOverflow) {
    u16errCnt++;
    #ifdef LOGGING
      Serial.print("MODBUS> ERR_BUFF_OVERFLOW");
      Serial.println();
    #endif
    logModbusRtu.warn("BUFOVER");
    return ERR_BUFF_OVERFLOW;
  }
  #ifdef LOGGING
    Serial.print("MODBUS> Buffer size: ");
    Serial.print(u8BufferSize);
    Serial.println();
  #endif
  return u8BufferSize;
}

/**
 * @brief
 * This method transmits au8Buffer to Serial line.
 * Change to TXEN in order to send, then change back
 * This is done with UCSRxA register.
 * The CRC is appended to the buffer before starting to send it.
 *
 * @param nothing
 * @return nothing
 * @ingroup buffer
 */
void Modbus::sendTxBuffer() {
  //uint8_t i = 0;
  #ifdef LOGGING
    Serial.print("MODBUS> Sending tx buffer");
    Serial.println();
  #endif

  // append CRC to message
  uint16_t u16crc = calcCRC( u8BufferSize );
  au8Buffer[ u8BufferSize ] = u16crc >> 8;
  u8BufferSize++;
  au8Buffer[ u8BufferSize ] = u16crc & 0x00ff;
  u8BufferSize++;

  // set RS485 transceiver to transmit mode
  #ifdef LOGGING
    Serial.print("MODBUS> sendTxBuffer -- ");
    for (uint8_t i = 0; i < u8BufferSize; i++) {
      Serial.print(au8Buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  #endif

  #ifdef LOGGING
    Serial.print("MODBUS> tx buffer set to transmit");
    Serial.println();
  #endif
  rxTxMode(TXEN);
  delayMicroseconds(100);

  // transfer buffer to serial line
  port->write( au8Buffer, u8BufferSize );

  // keep RS485 transceiver in transmit mode as long as sending
  port->flush();	//waits for transmittion to complete before returning

  // return RS485 transceiver to receive mode
  rxTxMode(RXEN);
  #ifdef LOGGING
    Serial.print("MODBUS> tx buffer switch back to rx mode");
    Serial.println();
  #endif

  //port->flush();
  u8BufferSize = 0;

  // set time-out for master
  u32timeOut = millis();

  // increase message counter
  u16OutCnt++;
}

/**
 * @brief
 * This method calculates CRC
 *
 * @return uint16_t calculated CRC value for the message
 * @ingroup buffer
 */
uint16_t Modbus::calcCRC(uint8_t u8length) {
  unsigned int temp, temp2, flag;
  temp = 0xFFFF;
  for (unsigned char i = 0; i < u8length; i++) {
    temp = temp ^ au8Buffer[i];
    for (unsigned char j = 1; j <= 8; j++) {
      flag = temp & 0x0001;
      temp >>=1;
      if (flag)
        temp ^= 0xA001;
    }
  }
  // Reverse byte order.
  temp2 = temp >> 8;
  temp = (temp << 8) | temp2;
  temp &= 0xFFFF;
  // the returned value is already swapped
  // crcLo byte is first & crcHi byte is last
  return temp;
}

/**
 * @brief
 * This method validates slave incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateRequest() {
  // check message crc vs calculated crc
  uint16_t u16MsgCRC =
    ((au8Buffer[u8BufferSize - 2] << 8)
    | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
  if ( calcCRC( u8BufferSize-2 ) != u16MsgCRC ) {
    u16errCnt ++;
    return NO_REPLY;
  }

  // check fct code
  boolean isSupported = false;
  for (uint8_t i = 0; i< sizeof( fctsupported ); i++) {
    if (fctsupported[i] == au8Buffer[FUNC]) {
      isSupported = 1;
      break;
    }
  }
  if (!isSupported) {
    u16errCnt ++;
    return EXC_FUNC_CODE;
  }

  // check start address & nb range
  uint16_t u16regs = 0;
  switch ( au8Buffer[ FUNC ] ) {
  case MB_FC_READ_COILS:
  case MB_FC_READ_DISCRETE_INPUT:
  case MB_FC_WRITE_MULTIPLE_COILS:
    u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]) / 16;
    u16regs += word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ]) /16;
    if (u16regs > u16regsize) {
      #ifdef LOGGING
        Serial.print("MODBUS> error regs size: u16regs, u16regsize: ");
        Serial.print(u16regs);
        Serial.print(" ");
        Serial.println(u16regsize);
      #endif
      return EXC_ADDR_RANGE;
    }
    break;
  case MB_FC_WRITE_COIL:
    u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]) / 16;
    if (u16regs > u16regsize) {
      #ifdef LOGGING
        Serial.print("MODBUS> error regs size: u16regs, u16regsize: ");
        Serial.print(u16regs);
        Serial.print(" ");
        Serial.println(u16regsize);
      #endif
      return EXC_ADDR_RANGE;
    }
    break;
  case MB_FC_WRITE_REGISTER :
    u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]);
    if (u16regs > u16regsize) {
      #ifdef LOGGING
        Serial.print("MODBUS> error regs size: u16regs, u16regsize: ");
        Serial.print(u16regs);
        Serial.print(" ");
        Serial.println(u16regsize);
      #endif
      return EXC_ADDR_RANGE;
    }
    break;
  case MB_FC_READ_REGISTERS :
  case MB_FC_READ_INPUT_REGISTER :
  case MB_FC_WRITE_MULTIPLE_REGISTERS :
    u16regs = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ]);
    u16regs += word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ]);
    if (u16regs > u16regsize) {
      #ifdef LOGGING
        Serial.print("MODBUS> error regs size: u16regs, u16regsize: ");
        Serial.print(u16regs);
        Serial.print(" ");
        Serial.println(u16regsize);
      #endif
      return EXC_ADDR_RANGE;
    }
    break;
  }
  return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method validates master incoming messages
 *
 * @return 0 if OK, EXCEPTION if anything fails
 * @ingroup buffer
 */
uint8_t Modbus::validateAnswer() {
  // check message crc vs calculated crc
  uint16_t u16MsgCRC =
    ((au8Buffer[u8BufferSize - 2] << 8)
    | au8Buffer[u8BufferSize - 1]); // combine the crc Low & High bytes
  if ( calcCRC( u8BufferSize-2 ) != u16MsgCRC ) {
    u16errCnt ++;
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("validateAnswer: NO_REPLY");
      Serial.println();
    #endif
      logModbusRtu.warn("VALNORPLY");
    return NO_REPLY;
  }

  // check exception
  if ((au8Buffer[ FUNC ] & 0x80) != 0) {
    u16errCnt ++;
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("validateAnswer: ERR_EXCEPTION");
      Serial.println();
    #endif
      logModbusRtu.warn("VALERREX");
    return ERR_EXCEPTION;
  }

  // check fct code
  boolean isSupported = false;
  for (uint8_t i = 0; i< sizeof( fctsupported ); i++) {
    if (fctsupported[i] == au8Buffer[FUNC]) {
      isSupported = 1;
      #ifdef LOGGING
        Serial.print("MODBUS> ");
        Serial.print("validateAnswer: fctsupported");
        Serial.println();
      #endif
      break;
    }
  }
  if (!isSupported) {
    u16errCnt ++;
    #ifdef LOGGING
      Serial.print("MODBUS> ");
      Serial.print("validateAnswer: EXC_FUNC_CODE");
      Serial.println();
    #endif
      logModbusRtu.warn("VALEXCFUNC");
    return EXC_FUNC_CODE;
  }

  #ifdef LOGGING
    Serial.print("MODBUS> ");
    Serial.print("validateAnswer: no issues");
    Serial.println();
  #endif

  return 0; // OK, no exception code thrown
}

/**
 * @brief
 * This method builds an exception message
 *
 * @ingroup buffer
 */
void Modbus::buildException( uint8_t u8exception ) {
  uint8_t u8func = au8Buffer[ FUNC ];  // get the original FUNC code

  au8Buffer[ ID ]      = u8id;
  au8Buffer[ FUNC ]    = u8func + 0x80;
  au8Buffer[ 2 ]       = u8exception;
  u8BufferSize         = EXCEPTION_SIZE;
}

/**
 * This method processes functions 1 & 2 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 * TODO: finish its implementation
 */
void Modbus::get_FC1() {
  uint8_t u8byte, i;
  u8byte = 3;

  #ifdef LOGGING
    Serial.print("MODBUS> FC1: ");
  #endif
    // only implements reading one byte at a time
  au16regs[0] = au8Buffer[u8byte];
  #ifdef LOGGING
    Serial.println();
  #endif
}

/**
 * This method processes functions 3 & 4 (for master)
 * This method puts the slave answer into master data buffer
 *
 * @ingroup register
 */
void Modbus::get_FC3() {
  uint8_t u8byte, i;
  u8byte = 3;

  #ifdef LOGGING
    Serial.print("MODBUS> FC3: ");
  #endif

  for (i=0; i< au8Buffer[ 2 ] /2; i++) {

    au16regs[ i ] = word(
      au8Buffer[ u8byte ],
      au8Buffer[ u8byte + 1 ]
    );

    #ifdef LOGGING
      Serial.print(au16regs[ i ], HEX);
      Serial.print(" ");
    #endif

    u8byte += 2;

  }
  #ifdef LOGGING
    Serial.println();
  #endif
}

/**
 * @brief
 * This method processes functions 1 & 2
 * This method reads a bit array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC1( uint16_t *regs, uint16_t u16size ) {
  uint8_t u8currentRegister, u8currentBit, u8bytesno, u8bitsno;
  uint8_t u8CopyBufferSize;
  uint16_t u16currentCoil, u16coil;

  // get the first and last coil from the message
  uint16_t u16StartCoil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
  uint16_t u16Coilno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );

  // put the number of bytes in the outcoming message
  u8bytesno = (uint8_t) (u16Coilno / 8);
  if (u16Coilno % 8 != 0) u8bytesno ++;
  au8Buffer[ ADD_HI ]  = u8bytesno;
  u8BufferSize         = ADD_LO;

  // read each coil from the register map and put its value inside the outcoming message
  u8bitsno = 0;

  for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++) {
    u16coil = u16StartCoil + u16currentCoil;
    u8currentRegister = (uint8_t) (u16coil / 16);
    u8currentBit = (uint8_t) (u16coil % 16);

    bitWrite(
    au8Buffer[ u8BufferSize ],
    u8bitsno,
    bitRead( regs[ u8currentRegister ], u8currentBit ) );
    u8bitsno ++;

    if (u8bitsno > 7) {
      u8bitsno = 0;
      u8BufferSize++;
    }
  }

  // send outcoming message
  if (u16Coilno % 8 != 0) u8BufferSize ++;
  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();
  return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes functions 3 & 4
 * This method reads a word array and transfers it to the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC3( uint16_t *regs, uint16_t u16size ) {

  uint16_t u16StartAdd = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
  uint16_t u16regsno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );
  uint8_t u8CopyBufferSize;
  uint16_t i;

  Serial.printlnf(" %u %u %u %u", u16StartAdd, u16regsno, u8CopyBufferSize, i);

  au8Buffer[ 2 ]       = u16regsno * 2;
  u8BufferSize         = 3;

  for (i = u16StartAdd; i < u16StartAdd + u16regsno; i++) {
    au8Buffer[ u8BufferSize ] = highByte(regs[i]);
    u8BufferSize++;
    au8Buffer[ u8BufferSize ] = lowByte(regs[i]);
    u8BufferSize++;
  }
  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();

  return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 5
 * This method writes a value assigned by the master to a single bit
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC5( uint16_t *regs, uint16_t u16size ) {
  uint8_t u8currentRegister, u8currentBit;
  uint8_t u8CopyBufferSize;
  uint16_t u16coil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );

  // point to the register and its bit
  u8currentRegister = (uint8_t) (u16coil / 16);
  u8currentBit = (uint8_t) (u16coil % 16);

  // write to coil
  bitWrite(
  regs[ u8currentRegister ],
  u8currentBit,
  au8Buffer[ NB_HI ] == 0xff );


  // send answer to master
  u8BufferSize = 6;
  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();

  return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 6
 * This method writes a value assigned by the master to a single word
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC6( uint16_t *regs, uint16_t u16size ) {

  uint8_t u8add = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
  uint8_t u8CopyBufferSize;
  uint16_t u16val = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );

  regs[ u8add ] = u16val;

  // keep the same header
  u8BufferSize         = RESPONSE_SIZE;

  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();

  return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 15
 * This method writes a bit array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup discrete
 */
int8_t Modbus::process_FC15( uint16_t *regs, uint16_t u16size ) {
  uint8_t u8currentRegister, u8currentBit, u8frameByte, u8bitsno;
  uint8_t u8CopyBufferSize;
  uint16_t u16currentCoil, u16coil;
  boolean bTemp;

  // get the first and last coil from the message
  uint16_t u16StartCoil = word( au8Buffer[ ADD_HI ], au8Buffer[ ADD_LO ] );
  uint16_t u16Coilno = word( au8Buffer[ NB_HI ], au8Buffer[ NB_LO ] );


  // read each coil from the register map and put its value inside the outcoming message
  u8bitsno = 0;
  u8frameByte = 7;
  for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++) {

    u16coil = u16StartCoil + u16currentCoil;
    u8currentRegister = (uint8_t) (u16coil / 16);
    u8currentBit = (uint8_t) (u16coil % 16);

    bTemp = bitRead(
    au8Buffer[ u8frameByte ],
    u8bitsno );

    bitWrite(
    regs[ u8currentRegister ],
    u8currentBit,
    bTemp );

    u8bitsno ++;

    if (u8bitsno > 7) {
      u8bitsno = 0;
      u8frameByte++;
    }
  }

  // send outcoming message
  // it's just a copy of the incomping frame until 6th byte
  u8BufferSize         = 6;
  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();
  return u8CopyBufferSize;
}

/**
 * @brief
 * This method processes function 16
 * This method writes a word array assigned by the master
 *
 * @return u8BufferSize Response to master length
 * @ingroup register
 */
int8_t Modbus::process_FC16( uint16_t *regs, uint16_t u16size ) {
  uint8_t u8func = au8Buffer[ FUNC ];  // get the original FUNC code
  uint16_t u16StartAdd = au8Buffer[ ADD_HI ] << 8 | au8Buffer[ ADD_LO ];
  uint8_t u8regsno = au8Buffer[ NB_HI ] << 8 | au8Buffer[ NB_LO ];
  uint8_t u8CopyBufferSize;
  uint16_t i;
  uint16_t temp;

  // build header
  au8Buffer[ NB_HI ]   = 0;
  au8Buffer[ NB_LO ]   = u8regsno;
  u8BufferSize         = RESPONSE_SIZE;

  // write registers
  for (i = 0; i < u8regsno; i++) {
    temp = word(
    au8Buffer[ (BYTE_CNT + 1) + i * 2 ],
    au8Buffer[ (BYTE_CNT + 2) + i * 2 ]);

    regs[ u16StartAdd + i ] = temp;
  }
  u8CopyBufferSize = u8BufferSize +2;
  sendTxBuffer();

  return u8CopyBufferSize;
}

// this switches between RXEN (0) and TXEN (1) modes
void Modbus::rxTxMode( uint8_t mode ) {
  if (mode == RXEN) {
    pins.digWrite(PIN_RS_RXEN, LOW);
    pins.digWrite(PIN_RS_TXEN, LOW);
    #ifdef LOGGING
      Serial.print("MODBUS> Changing to RX mode.");
      Serial.println();
    #endif
  } else {
    pins.digWrite(PIN_RS_RXEN, HIGH);
    pins.digWrite(PIN_RS_TXEN, HIGH);
    #ifdef LOGGING
      Serial.print("MODBUS> Changing to TX mode.");
      Serial.println();
    #endif
  }
  return;
};
