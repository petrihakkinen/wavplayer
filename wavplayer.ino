// Pin 3    Audio out (PWM)
// Pin 10   SD Card CS
// Pin 11   SD Card MOSI
// Pin 12   SD Card MISO
// Pin 13   SD Card CLK

#include <SPI.h>
#include <SD.h>

#define FILENAME	"credits.raw"
#define FILENAME	"theme.raw"
//#define FILENAME	"sine.raw"
#define BUFFER_SIZE	512

//#define TEST_SINE

File myFile;

volatile uint8_t sample[BUFFER_SIZE];
volatile uint16_t sample_pos = 0;

ISR(TIMER0_COMPA_vect) // called at 31250 Hz
{
	PORTD = sample[sample_pos];
	sample_pos++;
}

void playAudio()
{
	// global interrupt disable
	asm("cli");

	// set CPU clock prescaler to 1
	// isn't this the default setting?
	//CLKPR = 0x80;
	//CLKPR = 0x80;

	// DDRx Data Direction Register for Port x where x = C,D
	// each bit sets output mode for a pin
	//DDRC = 0x12; 
	DDRD = 0xff;
	//pinMode(3, OUTPUT);

	// this configures Timer/Counter0 to cause interrupts at 16 KHz
	// 16000000 Hz / 8 / 125 = 16000 Hz
	// 16000000 Hz / 8 / 64 = 31250 Hz
	TCCR0A = 2;  // set Clear Timer on Compare Match (CTC) mode
	TCCR0B = 2;  // set Timer/Counter clock prescaler to 1/8
	OCR0A = 64; // set Output Compare Register for Timer/Counter0 Comparator A

	// Enable Fast PWM
	// ===============
	// Timer/Counter2 Control Register A
	// bits 7-6: Clear OC2A on Compare Match, set OC2A at BOTTOM (non-inverting mode)
	// bits 5-4: Clear OC2B on Compare Match, set OC2B at BOTTOM (non-inverting mode)
	// bits 1-0: enable Fast PWM
	//TCCR2A = 0b10100011;
	// Timer/Counter2 Control Register A
	// bits 2-0: No prescaling (full clock rate)
	//TCCR2B = 0b00000001;

	// enables interrupt on Timer0
	// enable Timer/Counter0 Compare Match A interrupt
	TIMSK0 = 2;

	// global interrupt enable
	asm("sei");
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
	// TODO: arduino sd library runs at half spi speed, use sdfat library directly to get full speed!
	SD.begin(10);
	myFile = SD.open(FILENAME);

	// initial fill buffer
	myFile.read((void*)&sample[0], BUFFER_SIZE);

	// start playback
	playAudio();

	// test sine wave
#ifdef TEST_SINE
	for(int i = 0; i < BUFFER_SIZE; i++)
		sample[i] = (uint8_t)(sin(i * M_PI * 2.0f * 16.0f / BUFFER_SIZE) * 120.0f);
	while(true) {}
#endif

	setSckRate(0);	// full rate SPI

	while(true)
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
	}
}

void loop()
{
	// nothing
}
