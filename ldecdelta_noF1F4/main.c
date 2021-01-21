/*
 * ldecdelta
 *	Lokdecoder mit 1 fahrtrichtungsabh�ngigen Funktionsausgang f�r M�rklin digital mit AVR ATtiny24
 *	Mayer Bernhard, bernhard-mayer@gmx.net, www.blue-parrot.de
 *	V0.1
 *	23.Mai 2010
 
 * Delta-Adressen: (offen = 0b01; geschlossen = 0b00)
 *							Schalter:	   4 3 2 1
 *										  /|/|/|/|
 *	Dampflok	84		1---	---1	0b01010100
 *	Diesellok	80		12--	--21	0b01010000
 *	Triebwagen	68		1-3-	-3-1	0b01000100
 *	Elok		20		1--4	4--1	0b00010100

	FUSE-Bits f�r den ATtiny24 (internen Oszillator, kein CDIV8, Brownout bei 2,7 V):
	low: 0xe2
	high: 0xdd
	extended: 0xff

	-U lfuse:w:0xe2:m  -U hfuse:w:0xdd:m -U efuse:w:0xff:m
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define DIGSIG PB2
#define PINMA PA5
#define PINMB PA6
#define PINF0A PB0
#define PINF0B PB1
#define MAXBIT 18
//werte f�r 8 MHz
#define M1MIN 180
#define M1MAX 200
#define M0MIN 20
#define M0MAX 30
#define MPULSMIN 200
#define MPULSMAX 230

//variablen im EEPROM
uint8_t erichtung EEMEM;

//globale Variablen
volatile uint8_t timerflag;
volatile uint8_t adresse;
volatile uint8_t funktion;
volatile uint8_t geschw;
volatile uint8_t decoded;

void init_int(void)
{
	MCUCR=(1<<ISC00);						// jeder wechsel an INT0 erzeugt int;
	GIMSK=(1<<INT0);						// INT0 einschalten
}
void pwm_init(void)
{
	TCNT1=0;								// z�hler r�cksetzen
	TCCR1A=(1<<COM1A1)|(1<<COM1B1)|(1<<WGM10);		// Fast PWM, clear on compare match
	TCCR1B=(3<<CS10);						// prescaler 64 -> PWM frequenz 500 Hz
	OCR1A=0;
	OCR1B=0;
}
void timer0_init(void)
{
	TCNT0=0;								// z�hler r�cksetzen
	TIMSK0|=(1<<TOIE0);					// interrupt setzen;
	TCCR0A=0;
	TCCR0B=(2<<CS00);						// prescaler 8 und starten;
	timerflag=1;
}
void timer0_start(void)
{
	TCNT0=0;								// z�hler r�cksetzen
	TCCR0B=(2<<CS00);						// prescaler 8 und starten;
	timerflag=1;							// timerflag setzen
}
void timer0_stop(void)
{
	TCCR0B=0;								// stoppen;
	timerflag=0;							// timerflag r�cksetzen
}

int main(void)
{
	uint8_t tgeschw,richtung=0,f0=0, xadr=0, blockumschalt=0, lichtfest=0;
	DDRA=(1<<PINMA)|(1<<PINMB);			// PORTA als Eingang f�r M�useklavier
	PORTA=0xFF;								// aber pullups ein;
	DDRB=~(1<<DIGSIG);						// PORTB als ausgang f�r die funktionen
	PORTB=(1<<DIGSIG);						// PORTB pull up f�r Digitaleingang
	timer0_init();							// timer initialisieren
	if(!(PINA&(1<<4))) lichtfest=1;		// wenn SChalter 5 on, dann licht immer ein
	xadr=PINA&0x0F;
	xadr=((xadr&0x08)<<3)|((xadr&0x04)<<2)|((xadr&0x02)<<1)|(xadr&0x01);
	richtung=eeprom_read_byte(&erichtung);	// gespeicherte richtung vom eeprom holen

	init_int();								// interrupt einstellen
	pwm_init();								// PWM f�r Motoren einschalten
	decoded=0;	
	timerflag=0;
	sei();									// interrupts einschalten
	while(1)								// hauptschleife
	{
		if(decoded==1)						// g�ltige adresse decodiert
		{
			if(adresse!=0&&adresse==xadr)	// stimmt mit eigener adresse 
			{
				tgeschw=((geschw&0x40)>>3)+((geschw&0x10)>>2)+((geschw&0x04)>>1)+(geschw&0x01);
				f0=funktion?1:0;
				if(tgeschw==1)				// Umschaltung nach altem Protokoll; wird von 6604 verwendet
				{
					if(blockumschalt==0)	// mehrfache aussendung des befehls wird blockiert.
					{
						if(richtung==0) richtung=1;
						else richtung=0;
						eeprom_write_byte(&erichtung,richtung);		// richtung im eeprom speichern
					}
					tgeschw=0;				// geschw. auf 0 setzen, damit lok beim umschalten nicht f�hrt.
					blockumschalt=1;
				}
				else blockumschalt=0;
				
				if(f0|lichtfest)			// Funktion setzen wenn eingeschaltet oder fest eingeschaltet
				{
					if(richtung==1)
					{
						PORTB|=(1<<PINF0A);
						PORTB&=~(1<<PINF0B);
					}
					else
					{
						PORTB|=(1<<PINF0B);
						PORTB&=~(1<<PINF0A);
					}
				}
				else						// ansonten alles aus
				{
					PORTB&=~(1<<PINF0A);
					PORTB&=~(1<<PINF0B);
				}
				if(richtung==1)				// PWM setzen
				{
					OCR1A=0;
					OCR1B=(tgeschw<<4);
				}
				else
				{
					OCR1B=0;
					OCR1A=(tgeschw<<4);
				}
			}
			decoded=0;						// bereit f�r n�chste adresse
		}
	}
	return 0;
}

ISR(INT0_vect)
{
	uint8_t x;
	static uint8_t bitcount,wert;
	static uint8_t iadresse,ifunktion,igeschw;
	static uint8_t padresse,pfunktion,pgeschw;
	if(!(PINB&(1<<DIGSIG))) 				// pin = 0
	{
		wert=TCNT0;
		if(bitcount==MAXBIT-1)				// letzte negative Flanke
		{
			igeschw=igeschw>>1;
			if(wert>M1MIN&&wert<M1MAX) igeschw+=0x80;
			if(padresse==iadresse&&pfunktion==ifunktion&&pgeschw==igeschw)
			{
				adresse=iadresse;
				funktion=ifunktion;
				geschw=igeschw;
				iadresse=0;
				ifunktion=0;
				igeschw=0;
				if(decoded==0) decoded=1;
			}
			padresse=iadresse;
			pfunktion=ifunktion;
			pgeschw=igeschw;
			bitcount=0;
		}
	}
	else									// pin 1=0
	{
		x=TCNT0;
		TCNT0=0;							// timer r�cksetzen
		if(timerflag==0)
		{
			TCCR0B=(2<<CS00);				// Timer0 prescaler 8 und starten;
			timerflag=1;					// timerflag setzen
			bitcount=0;
		}
		if(x>(MPULSMIN)&&x<(MPULSMAX))		// g�ltiger wert f�r lokdecoder
		{
			if(bitcount==0)
			{
				iadresse=0;
				ifunktion=0;
				igeschw=0;
			}
			if(bitcount<8)					// Adresse
			{
				iadresse=iadresse>>1;
				if(wert>M1MIN&&wert<M1MAX) iadresse+=0x80;
			}
			else if(bitcount<10)			// Funktion
			{
				ifunktion=ifunktion>>1;
				if(wert>M1MIN&&wert<M1MAX) ifunktion+=0x80;
			}
			else if(bitcount<18)			// Geschwindigkeit
			{
				igeschw=igeschw>>1;
				if(wert>M1MIN&&wert<M1MAX) igeschw+=0x80;
			}
		
			bitcount++;
		}
		else bitcount=0;					// abbruch, da ung�ltig
	}
}
ISR(TIM0_OVF_vect)
{
	TCCR0B=0;								// Timer0 stoppen;
	timerflag=0;
}
