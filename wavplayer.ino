// Pin 3    Audio out (PWM)
// Pin 4    SD Card CS
// Pin 11   SD Card MOSI
// Pin 12   SD Card MISO
// Pin 13   SD Card CLK

#include <SPI.h>
#include <SD.h>

#define BUFFER_SIZE	512

File myFile;

uint8_t sample[BUFFER_SIZE];
uint16_t sample_pos = 0;

ISR(TIMER0_COMPA_vect) // called at 16 KHz
{
	OCR2B = sample[sample_pos] + 128;
	sample_pos++; // = (sample_pos + 1) & (BUFFER_SIZE-1);
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
	//DDRD = 0xff;
	pinMode(3, OUTPUT);

	// *** FIXME: setting any timer register messes up sd card library! ***

	// this configures Timer/Counter0 to cause interrupts at 16 KHz
	// 16000000 Hz / 8 / 125 = 16000 Hz
	TCCR0A = 2;  // set Clear Timer on Compare Match (CTC) mode
	TCCR0B = 2;  // set Timer/Counter clock prescaler to 1/8
	OCR0A = 125; // set Output Compare Register for Timer/Counter0 Comparator A

	// Enable Fast PWM
	// ===============
	// Timer/Counter2 Control Register A
	// bits 7-6: Clear OC2A on Compare Match, set OC2A at BOTTOM (non-inverting mode)
	// bits 5-4: Clear OC2B on Compare Match, set OC2B at BOTTOM (non-inverting mode)
	// bits 1-0: enable Fast PWM
	TCCR2A = 0b10100011;
	// Timer/Counter2 Control Register A
	// bits 2-0: No prescaling (full clock rate)
	TCCR2B = 0b00000001;

	// enables interrupt on Timer0
	// enable Timer/Counter0 Compare Match A interrupt
	TIMSK0 = 2;

	// global interrupt enable
	asm("sei");
}

/*
void loadSample()
{
	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	while(!Serial)
	{
		// wait for serial port to connect. Needed for native USB port only
	}

	if(!SD.begin(4))
	{
		Serial.println("initialization failed!");
		return;
	}
	Serial.println("initialization done.");

	Serial.println("Open file...");
	myFile = SD.open("wizard.raw");
	if(!myFile)
	{
		// if the file didn't open, print an error:
		Serial.println("error opening wizard.raw");
		return;
	}

	Serial.println("Loading sample...");
	for(int i = 0; i < BUFFER_SIZE; i++)
		sample[i] = myFile.read();

	//myFile.close();	
}
*/

void testSD()
{
	myFile = SD.open("test.txt");
	if(myFile)
	{
		Serial.println("test.txt:");
		while(myFile.available())
		{
			Serial.write(myFile.read());
		}
	}
	myFile.close();
}

void setup()
{
	Serial.begin(9600);
	SD.begin(4);
	//myFile = SD.open("wizard.raw");

	// while(true)
	// {
	// 	// fill buffer
	// 	for(int i = 0; i < BUFFER_SIZE; i++)
	// 		sample[i] = myFile.read();

	// 	// wait until buffer consumed
	// 	while(sample_pos < BUFFER_SIZE) {}
	// 	sample_pos = 0;
	// }
}

void loop()
{
	delay(1000);
	testSD();

	static int count = 0;
	count++;
	if(count == 3)
		playAudio();	// messes up SD card library!
}
