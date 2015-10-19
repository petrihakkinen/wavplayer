// PD0-PD7  8-bit audio out
// Pin 10   SD Card CS
// Pin 11   SD Card MOSI
// Pin 12   SD Card MISO
// Pin 13   SD Card CLK

#include <SPI.h>
#include <SD.h>

#define BUFFER_SIZE	512

//#define TEST_SINE

File myFile;

volatile uint8_t sample[BUFFER_SIZE];
volatile uint16_t sample_pos = 0;
uint16_t led = 0;

ISR(TIMER0_COMPA_vect) // called at 31250 Hz
{
	uint8_t s = sample[sample_pos];
	PORTD = s;
	sample_pos++;

	// flash led
	if((uint16_t)(s * s) > 40000)
		led = 65520;
	PORTC = (led>>6) > sample_pos;
	if(led > 0)
		led -= 16;
}

void setSckRate(uint8_t sckRateID)
{
	// see avr processor datasheet for SPI register bit definitions
	if ((sckRateID & 1) || sckRateID == 6)
		SPSR &= ~(1 << SPI2X);
	else
		SPSR |= (1 << SPI2X);
	SPCR &= ~((1 <<SPR1) | (1 << SPR0));
	SPCR |= (sckRateID & 4 ? (1 << SPR1) : 0) | (sckRateID & 2 ? (1 << SPR0) : 0);
}

void setup()
{
	DDRB = 0;		// set PB0 to input pull mode
	PORTB = 1;	

	DDRC = 1;

	DDRD = 0xff;

	SD.begin(10);

	int song = 0;

	// global interrupt disable
	asm("cli");

	// this configures Timer/Counter0 to cause interrupts at 16 KHz
	// 16000000 Hz / 8 / 125 = 16000 Hz
	// 16000000 Hz / 8 / 64 = 31250 Hz
	// 16000000 Hz / 8 / 50 = 40000 Hz
	TCCR0A = 2;  // set Clear Timer on Compare Match (CTC) mode
	TCCR0B = 2;  // set Timer/Counter clock prescaler to 1/8
	OCR0A = 50; // set Output Compare Register for Timer/Counter0 Comparator A

	// enable/disable Timer/Counter0 Compare Match A interrupt
	TIMSK0 = 0;

	// global interrupt enable
	asm("sei");

	while(true)
	{
		// next filename
		char filename[16];
		while(true)
		{
			sprintf(filename, "tune%d.raw", song + 1);
			if(SD.exists(filename))
				break;
			song = 0;
		}

		// open file
		myFile = SD.open(filename);

		// initial fill buffer
		myFile.read((void*)&sample[0], BUFFER_SIZE);

		// test sine wave
#ifdef TEST_SINE
		for(int i = 0; i < BUFFER_SIZE; i++)
			sample[i] = (uint8_t)(sin(i * M_PI * 2.0f * 16.0f / BUFFER_SIZE) * 120.0f);
		while(true) {}
#endif

		// full rate SPI
		setSckRate(0);

		TIMSK0 = 2;	// enable audio interrupt

		uint8_t prev_button = 0;

		while(myFile.available() > 0)
		{
			// wait until first half consumed
			while(sample_pos < BUFFER_SIZE/2) {}

			// load data
			myFile.read((void*)&sample[0], BUFFER_SIZE/2);

			// led on if took too look
			//if(sample_pos > 1023)
			//	PORTD |= 16;

			// wait until second half consumed
			while(sample_pos < BUFFER_SIZE) {}
			sample_pos = 0;

			// load data
			myFile.read((void*)&sample[BUFFER_SIZE/2], BUFFER_SIZE/2);

			// led on if took too look
			//if(sample_pos > 511)
			//	PORTD |= 16;

			// skip song when button is pressed
			uint8_t button = (PINB & 1);
			if(button == 0 && prev_button != 0)
			{
				TIMSK0 = 0;	// disable audio interrupt

				for(uint16_t i = 0; i < 50000; i++)	// debounce delay
				{
					__asm__ __volatile__ (
						".rept 10\n\t"
						"nop\n\t"
						".endr"
						:
						:
						:
					);
				}
				break;
			}
			prev_button = button;
		}

		TIMSK0 = 0;	// disable audio interrupt

		myFile.close();
		song++;
	}
}

void loop()
{
	// nothing
}
