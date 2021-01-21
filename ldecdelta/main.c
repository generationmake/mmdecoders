/*
 * ldecdelta
 *	Lokdecoder mit 1 fahrtrichtungsabhängigen Funktionsausgang für Märklin digital mit AVR ATtiny24
 *	basierend auf Protokoll version 2 (MM2) mit 255 Adressen und 4 Funktionen; kompatibel zu Version 1 (MM1)
 *	Mayer Bernhard, bernhard-mayer@gmx.net, www.blue-parrot.de
 *	V0.1
	03. Juni 2010
 
 * Delta-Adressen: (offen = 0b01; geschlossen = 0b00)
 *							Schalter:	   4 3 2 1
 *										  /|/|/|/|
 *	Dampflok	84		1---	---1	0b01010100
 *	Diesellok	80		12--	--21	0b01010000
 *	Triebwagen	68		1-3-	-3-1	0b01000100
 *	Elok		20		1--4	4--1	0b00010100
 *	Delta Pilot	00		1234	4321	0b00000000

	FUSE-Bits für den ATtiny24 (internen Oszillator, kein CDIV8, Brownout bei 2,7 V):
	low: 0xe2
	high: 0xdd
	extended: 0xff

	-U lfuse:w:0xe2:m  -U hfuse:w:0xdd:m -U efuse:w:0xff:m
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <avr/sleep.h>

#define DIGSIG PB2
#define PINMA PA5
#define PINMB PA6
#define PINF0A PB0
#define PINF0B PB1
#define MAXBIT 18
//werte für 8 MHz
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
	TCNT1=0;								// zähler rücksetzen
	TCCR1A=(1<<COM1A1)|(1<<COM1B1)|(1<<WGM10);		// Fast PWM, clear on compare match
	TCCR1B=(2<<CS10);						// prescaler 64 -> PWM frequenz 500 Hz
	OCR1A=0;
	OCR1B=0;
}
void timer0_init(void)
{
	TCNT0=0;								// zähler rücksetzen
	TIMSK0|=(1<<TOIE0);					// interrupt setzen;
	TCCR0A=0;
	TCCR0B=(2<<CS00);						// prescaler 8 und starten;
	timerflag=1;
}
void timer0_start(void)
{
	TCNT0=0;								// zähler rücksetzen
	TCCR0B=(2<<CS00);						// prescaler 8 und starten;
	timerflag=1;							// timerflag setzen
}
void timer0_stop(void)
{
	TCCR0B=0;								// stoppen;
	timerflag=0;							// timerflag rücksetzen
}

int main(void)
{
	uint8_t tgeschw,HGFE,richtung=0,f0=0,f1=0,f2=0,f3=0,f4=0, xadr=0, blockumschalt=0, lichtfest=0;
	DDRA=(1<<PINMA)|(1<<PINMB);			// PORTA als Eingang für Mäuseklavier
	PORTA=0b10011111;						// pullups ein für Mäuseklavier;
	DDRB=~(1<<DIGSIG);						// PORTB als ausgang für die funktionen
	PORTB=(1<<DIGSIG);						// PORTB pull up für Digitaleingang
	timer0_init();							// timer initialisieren
	if(!(PINA&(1<<4))) lichtfest=1;		// wenn SChalter 5 on, dann licht immer ein
	xadr=PINA&0x0F;
	xadr=((xadr&0x08)<<3)|((xadr&0x04)<<2)|((xadr&0x02)<<1)|(xadr&0x01);
	richtung=eeprom_read_byte(&erichtung);	// gespeicherte richtung vom eeprom holen
	PORTA=0;								// PORTA pullups wieder aus zum Strom sparen

	init_int();								// interrupt einstellen
	pwm_init();								// PWM für Motoren einschalten
	decoded=0;	
	timerflag=0;
	set_sleep_mode(SLEEP_MODE_IDLE);		// Sleep mode setzen
	sleep_enable();							// aktivieren
	sei();									// interrupts einschalten
	while(1)								// hauptschleife
	{
		if(decoded==1)						// gültige adresse decodiert
		{
			if(adresse==xadr)	// stimmt mit eigener adresse überein
//			if(adresse!=0&&adresse==xadr)	// stimmt mit eigener adresse überein
			{
				HGFE=((geschw&0x80)>>4)+((geschw&0x20)>>3)+((geschw&0x08)>>2)+((geschw&0x02)>>1);
				tgeschw=((geschw&0x40)>>3)+((geschw&0x10)>>2)+((geschw&0x04)>>1)+(geschw&0x01);
				if(tgeschw!=HGFE)			// test, ob neues format
				{
					if(HGFE==0b00000101)
					{
						if(tgeschw>6) richtung=0;
						else if(tgeschw==2) f1=0;	// hier avtl alle +1, da 1 keine fahrstufe????
						else if(tgeschw==3) f2=0;
						else if(tgeschw==5) f3=0;
						else if(tgeschw==6) f4=0;
					}
					else if(HGFE==0b00001010)
					{
						if(tgeschw<=6) richtung=1;
						else if(tgeschw==10) f1=1;	// hier avtl alle +1, da 1 keine fahrstufe????
						else if(tgeschw==11) f2=1;
						else if(tgeschw==13) f3=1;
						else if(tgeschw==14) f4=1;
					}
					else if(HGFE==0b00001101&&tgeschw<=6) richtung=0;
					else if(HGFE==0b00000010&&tgeschw>6) richtung=1;
					else if(HGFE==0b00000011) f1=0;
					else if(HGFE==0b00001011) f1=1;
					else if(HGFE==0b00000100) f2=0;
					else if(HGFE==0b00001100) f2=1;
					else if(HGFE==0b00000110) f3=0;
					else if(HGFE==0b00001110) f3=1;
					else if(HGFE==0b00000111) f4=0;
					else if(HGFE==0b00001111) f4=1;
				}
				f0=funktion?1:0;
				if(tgeschw==1)				// Umschaltung nach altem Protokoll; wird von 6604 verwendet
				{
					if(blockumschalt==0)	// mehrfache aussendung des befehls wird blockiert.
					{
						if(richtung==0) richtung=1;
						else richtung=0;
						eeprom_write_byte(&erichtung,richtung);		// richtung im eeprom speichern
					}
					tgeschw=0;				// geschw. auf 0 setzen, damit lok beim umschalten nicht fährt.
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
			decoded=0;						// bereit für nächste adresse
		}
		sleep_cpu();						// in den sleep mode
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
		TCNT0=0;							// timer rücksetzen
		if(timerflag==0)
		{
			TCCR0B=(2<<CS00);				// Timer0 prescaler 8 und starten;
			timerflag=1;					// timerflag setzen
			bitcount=0;
		}
		if(x>(MPULSMIN)&&x<(MPULSMAX))		// gültiger wert für lokdecoder
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
		else bitcount=0;					// abbruch, da ungültig
	}
}
ISR(TIM0_OVF_vect)
{
	TCCR0B=0;								// Timer0 stoppen;
	timerflag=0;
}
