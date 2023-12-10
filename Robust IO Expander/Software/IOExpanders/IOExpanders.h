#ifndef IO_EXPANDERS_H
#define IO_EXPANDERS_H

#include <SPI.h>


class IOExpanders
{
private:
	SPISettings spi_settings; 

	static const uint8_t MAX_NUMBER_EXPANDERS = 4;
	static const uint8_t MAX_NUMBER_WRITES = 64;
	
	struct IOExpander
	{
		bool enabled;
		uint8_t chip_select_pin;
		unsigned int pin_mode;
		unsigned int pullup_mode;
		
		//unsigned int output_cache;
		//bool has_output_changed;
		
		// Writes are cued and ensured
		unsigned int write_cue[MAX_NUMBER_WRITES];
		unsigned int write_cue_index;
		unsigned int last_write;
		
		unsigned int input_cache;
	};
	typedef struct IOExpander IOExpander;
	IOExpander expanders[MAX_NUMBER_EXPANDERS];
	uint8_t number_of_expanders;
	uint8_t last_used_chip;
	unsigned long last_read_write;
	
	bool enabled; // Have we begun?
	
	// Constants for the MCP23S17 chip
	const uint8_t OPCODEW = 0b01000000;			// Opcode for MCP23S17 with LSB (bit0) set to write (0), address OR'd in later, bits 1-3
	const uint8_t OPCODER = 0b01000001;			// Opcode for MCP23S17 with LSB (bit0) set to read (1), address OR'd in later, bits 1-3
	const uint8_t ADDR_ENABLE = 0b00001000;		// Configuration register for MCP23S17, the only thing we change is enabling hardware addressing
	
	const uint8_t MSG_IOCON = 0x0A;				// MCP23x17 Configuration Register
	const uint8_t MSG_IODIRA = 0x00;
	const uint8_t MSG_GPPUA = 0x0C;
	const uint8_t MSG_GPIOA = 0x12; 
	
	#if defined(__IMXRT1062__) // Teensy 4.x
		const unsigned short chipSelectWaitMs = 10;
	#else
		const unsigned short chipSelectWaitMs = 1;
	#endif

public:
	IOExpanders();
	
	/*
	* Add an IO Expander chip to the list
	*/
	void Add( uint8_t _address, uint8_t _chip_select_pin );
	
	/*
	* Start listening to the expanders
	*/
	void Enable( bool _enabled );
	
	/*
	* Called every cycle
	*/
	void Tick();
	
	/*
	* Sets the mode (input or output) of all I/O pins at once 
	*/
	void PinMode( uint8_t id, unsigned int mode );

	/*
	* Selects internal 100k input pull-up of all I/O pins at once
	*/
	void PullupMode( uint8_t id, unsigned int mode );
	
	/*
	* Write a byte to a given SPI Expander
	*/
	void ByteWrite( uint8_t id, uint8_t reg, uint8_t value );
	
	/*
	* Write a word to a given SPI Expander
	*/
	void WordWrite( uint8_t id, uint8_t reg, unsigned int word );
	
	/*
	* Change the state of a given pin
	*/
	//void Write( uint8_t id, uint8_t pin, uint8_t value, bool force = false );
	
	/*
	* Change the state of a given pin in due course
	*/
	void WriteCue( uint8_t id, uint8_t pin, uint8_t value );
	
	
	/*
	* Read the entire state of a chip
	*/
	unsigned int Read( uint8_t id );
	
	/*
	* Read the state of a pin
	*/
	uint8_t Read( uint8_t id, uint8_t pin );
	
};
#endif