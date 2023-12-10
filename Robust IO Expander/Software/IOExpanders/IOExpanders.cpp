#include "IOExpanders.h"

IOExpanders::IOExpanders() : spi_settings(1000000, MSBFIRST, SPI_MODE0)
{
	enabled = false;
	number_of_expanders = 0;
	last_used_chip = 0;
	last_read_write = 0;
	
	// Clear expanders
	for ( uint8_t i = 0; i < MAX_NUMBER_EXPANDERS; i++ )
	{
		expanders[i].enabled = false;
	}
};

/*
 * Add an IO Expander chip to the list
 */
void IOExpanders::Add( uint8_t _address, uint8_t _chip_select_pin )
{
	// Setup the data structure
	expanders[_address].enabled = true;
	expanders[_address].chip_select_pin = _chip_select_pin;
	expanders[_address].pin_mode = 0xFFFF;
	expanders[_address].pullup_mode = 0x0000;
	//expanders[_address].output_cache = 0x0000;
	//expanders[_address].has_output_changed = false;
	expanders[_address].input_cache = 0x0000;
	
	// Clear write cue
	for ( uint8_t j = 0; j < MAX_NUMBER_WRITES; j++ )
	{
		expanders[_address].write_cue[j] = 0x0000;
	}
	expanders[_address].write_cue_index = 0;
	expanders[_address].last_write = 0x0000;
	
	// Setup the pins
	pinMode( _chip_select_pin, OUTPUT ); // Set SlaveSelect pin as an output
	digitalWriteFast( _chip_select_pin, 1 ); // Set SlaveSelect HIGH (chip de-selected)
	
	// Enable the chip
	ByteWrite( _address, MSG_IOCON, ADDR_ENABLE);
}

/*
 * Start listening to the expanders
 */
void IOExpanders::Enable( bool _enabled )
{
	enabled = _enabled;
}

/*
 * Called every cycle
 */
void IOExpanders::Tick()
{
	// Are we enabled?
	if (!enabled) return;
	
	// 1khz
	if ( millis() - last_read_write < 10 ) return;
	//if ( millis() - last_read_write < 1000 ) return;
	last_read_write = millis();
	
	// Loop through expanders
	uint8_t i = last_used_chip+1;
	uint8_t iterations = 0;
	while ( i < MAX_NUMBER_EXPANDERS && iterations < 2 )
	{
		// Hit the end and didn't trigger
		if (i >= MAX_NUMBER_EXPANDERS-1)
		{
			i = 0;
			iterations++;
		}
		
		// In use?
		if (expanders[i].enabled)
		{
			// Do we need to write?
			if ( expanders[i].write_cue_index > 0 )
			{
				// Send new write data
				WordWrite( i, MSG_GPIOA, expanders[i].write_cue[0] );
				expanders[i].last_write = expanders[i].write_cue[0];
				
				// Shuffle the buffer
				//  It holds the history so we only shuffle it when writing into it
				for ( uint8_t j = 0; j < MAX_NUMBER_WRITES-1; j++ ) {
					expanders[i].write_cue[j] = expanders[i].write_cue[j+1];
				}
				
				// Reduce cue length
				expanders[i].write_cue_index--;
				
				break; // Don't read
			}
			

			// Read the current state
			// This function will read all 16 bits of I/O, and return them as a word in the format 0x(portB)(portA)
			expanders[i].input_cache = 0;                   		// Initialize a variable to hold the read values to be returned
			
			SPI.beginTransaction( spi_settings );

			digitalWriteFast( expanders[i].chip_select_pin, 0 );	// Take slave-select low
				delayMicroseconds(chipSelectWaitMs);
				SPI.transfer( OPCODER | (i << 1) );						// Send the MCP23S17 opcode, chip address, and read bit
				SPI.transfer( MSG_GPIOA );								// Send the register we want to read
				expanders[i].input_cache = SPI.transfer(0x00);			// Send any byte, the function will return the read value (register address pointer will auto-increment after write)
				expanders[i].input_cache |= (SPI.transfer(0x00) << 8);	// Read in the "high byte" (portB) and shift it up to the high location and merge with the "low byte"
			digitalWriteFast( expanders[i].chip_select_pin, 1 );	// Take slave-select high
			delayMicroseconds(chipSelectWaitMs);

			SPI.endTransaction();
			
			/*
			Serial.print( "(" );
			Serial.print( i );
			Serial.print( ") Read " );
			Serial.println( expanders[i].input_cache );
			*/
			
			break;

		}
		
		// Increase
		i++;
	}
	
	// Update the last used chip
	last_used_chip = i;
}

/*
 * Sets the mode (input or output) of all I/O pins at once 
 */
void IOExpanders::PinMode( uint8_t id, unsigned int mode )
{
	WordWrite( id, MSG_IODIRA, mode ); // Call the the generic word writer with start register and the mode cache
	expanders[id].pin_mode = mode;
}

/*
 * Selects internal 100k input pull-up of all I/O pins at once
 */
void IOExpanders::PullupMode( uint8_t id, unsigned int mode )
{
	WordWrite( id, MSG_GPPUA, mode );
	expanders[id].pullup_mode = mode;
}

/*
 * Write a byte to a given SPI Expander
 */
void IOExpanders::ByteWrite( uint8_t id, uint8_t reg, uint8_t value ) {
	// Accept the register and byte
	SPI.beginTransaction( spi_settings );
	
	digitalWriteFast( expanders[id].chip_select_pin, 0 );						// Begin SPI conversation
		delayMicroseconds(chipSelectWaitMs);
		SPI.transfer( (OPCODEW | id << 1) );	// Send the MCP23S17 opcode, chip address, and write bit
		SPI.transfer( reg );
		SPI.transfer( value );														// Send the byte
	digitalWriteFast( expanders[id].chip_select_pin, 1 );						// Terminate SPI conversation
	delayMicroseconds(chipSelectWaitMs);

	SPI.endTransaction();
}

/*
 * Write a word to a given SPI Expander
 */
void IOExpanders::WordWrite( uint8_t id, uint8_t reg, unsigned int word ) {
	// Accept the start register and word 
	SPI.beginTransaction( spi_settings );
	
	digitalWriteFast( expanders[id].chip_select_pin, 0 );						// Take slave-select low
		delayMicroseconds(chipSelectWaitMs);
		SPI.transfer( OPCODEW | (id << 1) );	// Send the MCP23S17 opcode, chip address, and write bit
		SPI.transfer( reg );														// Send the register we want to write 
		SPI.transfer( (uint8_t) (word) );                      						// Send the low byte (register address pointer will auto-increment after write)
		SPI.transfer( (uint8_t) (word >> 8) );                 						// Shift the high byte down to the low byte location and send
	digitalWriteFast( expanders[id].chip_select_pin, 1 );						// Take slave-select high
	delayMicroseconds(chipSelectWaitMs);
	
	SPI.endTransaction();
}

/*
 * Change the state of a given pin
 */
/*
void IOExpanders::Write( uint8_t id, uint8_t pin, uint8_t value, bool force )
{
	// Valid pin?
	if ( pin < 1 || pin > 16 ) return;

	unsigned int output_cache = expanders[id].output_cache;
	if (value) {
		expanders[id].output_cache |= 1 << (pin - 1);
	} else {
		expanders[id].output_cache &= ~(1 << (pin - 1));
	}

	expanders[id].has_output_changed = false;
	if ( expanders[id].output_cache != output_cache ) expanders[id].has_output_changed = true;

	
	//Serial.print( "Initial Write " );
	//Serial.println( expanders[id].output_cache );
	//if (expanders[id].has_output_changed)
	//{
	//	Serial.println( "Changed" );
	//}
	
				
	// Write this straight away?
	if (force) WordWrite(id, MSG_GPIOA, expanders[id].output_cache);
}
*/

/*
 * Change the state of a given pin, add it to the cue
 */
void IOExpanders::WriteCue( uint8_t id, uint8_t pin, uint8_t value )
{
	// Valid pin?
	if ( pin < 1 || pin > 16 ) return;
	
	// Cue full?
	if ( expanders[id].write_cue_index >= MAX_NUMBER_WRITES ) return;
	
	// Anything in the write cue?
	// Grab the top of the cue, even if its empty
	unsigned int output;
	if ( expanders[id].write_cue_index == 0 ) {
		output = expanders[id].last_write;
	} else {
		output = expanders[id].write_cue[expanders[id].write_cue_index-1];
	}
	
	/*
	Serial.print( "Pin: " );
	Serial.print( pin );
	Serial.print( ", Value: " );
	Serial.print( value );
	Serial.print( ", Index: " );
	Serial.print( expanders[id].write_cue_index );
	Serial.print( ", Historic Output: " );
	Serial.print( output );
	*/
	
	// Change the write state
	if (value) {
		output |= 1 << (pin - 1);
	} else {
		output &= ~(1 << (pin - 1));
	}
	/*
	Serial.print( ", New Output: " );
	Serial.println( output );
	*/
	
	// Has the output changed? If not, break out
	if ( expanders[id].write_cue_index > 0 )
	{
		if ( output == expanders[id].write_cue[expanders[id].write_cue_index-1] ) {
			return;
		}
	}
	
	// Add to cue
	expanders[id].write_cue[ expanders[id].write_cue_index ] = output;
	
	// Increase the write cue index
	expanders[id].write_cue_index++;
}


/*
 * Read the entire state of a chip
 */
unsigned int IOExpanders::Read( uint8_t id )
{
	return expanders[id].input_cache;
}

/*
 * Read the state of a pin
 */
uint8_t IOExpanders::Read( uint8_t id, uint8_t pin )
{
	//Serial.print( "Read Result: " );
	//Serial.println( (1 << (pin - 1)) ? 1 : 0 );
	
	return expanders[id].input_cache & (1 << (pin - 1)) ? 1 : 0;
}
