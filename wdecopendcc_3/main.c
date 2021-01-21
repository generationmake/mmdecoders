/*
 *	Grundroutinen für einen Märklin digital decoder auf AVR
 *	basierend auf Protokoll version für Magnetartikel
 *	Mayer Bernhard, bernhard-mayer@gmx.net, www.blue-parrot.de
 *	V0.2
 *	27.12.2009
 * 	für die Hardware von www.opendcc.de (Var. 1) mit ATtiny2313
 *	Fusebits: Standard, kein CKDIV8, BODLEVEL=101 (2,7 V)
 *	-> lfuse: 0xe4, hfuse: 0xdb, efuse: 0xff
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define BAUD 38400UL
#define UBRR_VAL ((F_CPU+BAUD*8)/(BAUD*16)-1)

#define MAXBIT 18
//Werte für Magnetartikel
#define M1MIN 70
#define M1MAX 90
#define M0MIN 20
#define M0MAX 30
// alle ausgangspins definieren
#define PINPROG PD0
#define PORTPROG PORTD
#define PPINPROG PIND
#define DDRPROG DDRD
#define PINLED PD6
#define PORTLED PORTD
#define DDRLED DDRD
#define PIN10 PB0
#define PORT10 PORTB
#define DDR10 DDRB
#define PIN11 PB1
#define PORT11 PORTB
#define DDR11 DDRB
#define PIN20 PB2
#define PORT20 PORTB
#define DDR20 DDRB
#define PIN21 PB3
#define PORT21 PORTB
#define DDR21 DDRB
#define PIN30 PB4
#define PORT30 PORTB
#define DDR30 DDRB
#define PIN31 PB5
#define PORT31 PORTB
#define DDR31 DDRB
#define PIN40 PB6
#define PORT40 PORTB
#define DDR40 DDRB
#define PIN41 PB7
#define PORT41 PORTB
#define DDR41 DDRB



uint8_t saveadresse EEMEM;

volatile uint8_t timerflag;
//volatile uint8_t x0[72];
volatile uint8_t adresse;
volatile uint8_t funktion;
volatile uint8_t ausgang;
volatile uint8_t decoded;

void uart_init(void)
{
	UCSRB |= (1<<TXEN);		// nur senden wichtig
//	UCSRB |= (1<<TXEN)|(1<<RXEN)|(1<<RXCIE);
	UCSRC |= (3<<UCSZ0);
//	UCSRC |= (1<<URSEL)|(3<<UCSZ0);
	
//	UBRRH = UBRR_VAL >>8;
//	UBRRL = UBRR_VAL &0xFF;
	UBRRH = 0;
	UBRRL = 12;
}

int uart_putc(unsigned char c)	// zeichen am uart ausgeben
{
	while(!(UCSRA&(1<<UDRE)))
	{
	}
	UDR = c;
	return 0;
}

void uart_puts(char *s)			// null-terminierten string am uart ausgeben
{
	while(*s)
	{
		uart_putc(*s);
		s++;
	}
}
void uart_bcd8(uint8_t x)		// wert bcd kodiert am uart ausgeben
{
	uart_putc((x/100)+48);
	uart_putc(((x/10)%10)+48);
	uart_putc((x%10)+48);
}

void init_int(void)
{
	MCUCR=(1<<ISC00);			// jeder wechsel an INT0 erzeugt int;
	GIMSK=(1<<INT0);			// INT0 einschalten
}
void timer0_init(void)
{
	TCNT0=0;					// zähler rücksetzen
	TIMSK|=(1<<TOIE0);			// interrupt setzen;
	TCCR0B=0;
	TCCR0B=(2<<CS00);			// prescaler 8 und starten;
	timerflag=1;
}
void timer0_start(void)
{
	TCNT0=0;					// zähler rücksetzen
	TCCR0B=(2<<CS00);			// prescaler 8 und starten;
	timerflag=1;				// timerflag setzen
}
void timer0_stop(void)
{
	TCCR0B=0;					// stoppen;
	timerflag=0;				// timerflag rücksetzen
}

int main(void)
{
	uint8_t j, xadr=0;
	DDRD=0;									// PORTD als eingang für dig-signal und progtaster
	PORTD=0xFF;								// pull up ein, definierter pegel für INT0
	PORTPROG|=(1<<PINPROG);				// pull up ein für Prog Taster
	DDRLED|=(1<<PINLED);					// LED als Ausgang
	PORTLED|=(1<<PINLED);					// LED einschalten
	DDR10|=(1<<PIN10);						// Funktionspins als Ausgänge
	DDR11|=(1<<PIN11);
	DDR20|=(1<<PIN20);
	DDR21|=(1<<PIN21);
	DDR30|=(1<<PIN30);
	DDR31|=(1<<PIN31);
	DDR40|=(1<<PIN40);
	DDR41|=(1<<PIN41);
	timer0_init();							// timer initialisieren
	uart_init();							// uart für debug ausgabe
	xadr=eeprom_read_byte(&saveadresse);	// gespeicherte adresse vom eeprom holen
//	xadr=0x0c;	// gespeicherte adresse vom eeprom holen
	uart_puts("Maerklin digital decoder, (c) 2009\r\n");
	uart_puts("Eigene adresse: ");
	uart_bcd8(xadr);
	uart_puts("\r\n");
	init_int();								// interrupt einstellen
	decoded=0;	
	timerflag=0;
	PORTLED&=~(1<<PINLED);					// LED wieder ausschalten
	sei();									// interrupts einschalten
	while(1)								// hauptschleife
	{
		if(!(PPINPROG&(1<<PINPROG)))			// programmiertaste gedrückt
		{
			j=0;
			while(j==0)
			{
				if(!(PORTLED&(1<<PINLED))) PORTLED|=(1<<PINLED);	// LED blinken lassen
				else PORTLED&=~(1<<PINLED);
				if(decoded==1)				// gültige adresse dekodiert
				{
					if(adresse!=0xFF)			// und ungleich 0xFF
					{
						xadr=adresse;		// abholen
						uart_puts("neue adresse: ");
						uart_bcd8(adresse);
						uart_puts("\r\n");
						eeprom_write_byte(&saveadresse,xadr);	// und speichern
						j=1;				//schleifenbedingung beenden
					}
					decoded=0;
				}
				_delay_ms(50);				// verzögerung für blinken
			}
			PORTLED&=~(1<<PINLED);			// LED wieder ausschalten
		}
		if(decoded==1)						// gültige adresse decodiert
		{
			uart_bcd8(adresse);
			uart_putc('-');
			uart_bcd8(funktion);
			uart_putc('-');
			uart_bcd8(ausgang);
			uart_putc('-');
			if(adresse==xadr)	// stimmt mit eigener adresse überein
			{
/*				for(j=0;j<18;j++)
				{
//					uart_putc(' ');
//					uart_bcd8(x);
				}
*/				if(((ausgang&0x55)==((ausgang>>1)&0x55))&&funktion==0)	// werte gültig
				{
					uart_putc('g');
					if(ausgang==0b11000000)
					{
						PORT10|=(1<<PIN10);
						PORT11&=~(1<<PIN11);
					}
					if(ausgang==0b11000011)
					{
						PORT11|=(1<<PIN11);
						PORT10&=~(1<<PIN10);
					}
					if(ausgang==0b11001100)
					{
						PORT20|=(1<<PIN20);
						PORT21&=~(1<<PIN21);
					}
					if(ausgang==0b11001111)
					{
						PORT21|=(1<<PIN21);
						PORT20&=~(1<<PIN20);
					}
					if(ausgang==0b11110000)
					{
						PORT30|=(1<<PIN30);
						PORT31&=~(1<<PIN31);
					}
					if(ausgang==0b11110011)
					{
						PORT31|=(1<<PIN31);
						PORT30&=~(1<<PIN30);
					}
					if(ausgang==0b11111100)
					{
						PORT40|=(1<<PIN40);
						PORT41&=~(1<<PIN41);
					}
					if(ausgang==0b11111111)
					{
						PORT41|=(1<<PIN41);
						PORT40&=~(1<<PIN40);
					}
				}
			}
			decoded=0;				// bereit für nächste adresse
			uart_puts("\r\n");
		}
	}
	return 0;
}

ISR(INT0_vect)
{
	uint8_t x;
	static uint8_t bitcount,wert;
	static uint8_t iadresse,ifunktion,iausgang;
	static uint8_t padresse,pfunktion,pausgang;
		
	if(!(PIND&(1<<2))) // pin 0
	{
		wert=TCNT0;
//		x0[bitcount]=wert;
	}
	else				// pin 1
	{
		x=TCNT0;
		TCNT0=0;		// timer rücksetzen
		if(timerflag==0)
		{
			TCCR0B=(2<<CS00);			// Timer0 prescaler 8 und starten;
			timerflag=1;				// timerflag setzen
			bitcount=0;
		}
		if(x>100&&x<115)
		{
			if(bitcount==0)
			{
				iadresse=0;
				ifunktion=0;
				iausgang=0;
			}
			if(bitcount<8)
			{
				iadresse=iadresse>>1;
				if(wert>M1MIN&&wert<M1MAX) iadresse+=0x80;
			}
			else if(bitcount<10)
			{
				ifunktion=ifunktion>>1;
				if(wert>M1MIN&&wert<M1MAX) ifunktion+=0x80;
			}
			else if(bitcount<18)
			{
				iausgang=iausgang>>1;
				if(wert>M1MIN&&wert<M1MAX) iausgang+=0x80;
			}
			bitcount++;
			if(bitcount==18)
			{
				if(padresse==iadresse&&pfunktion==ifunktion&&pausgang==iausgang)
				{
					adresse=iadresse;
					funktion=ifunktion;
					ausgang=iausgang;
					iadresse=0;
					ifunktion=0;
					iausgang=0;
					if(decoded==0)	decoded=1;
				}
				padresse=iadresse;
				pfunktion=ifunktion;
				pausgang=iausgang;
				bitcount=0;
			}
		}
		else bitcount=0;
	}
}
ISR(TIMER0_OVF_vect)
{
	TCCR0B=0;					// Timer0 stoppen;
	timerflag=0;				// timerflag rücksetzen
}
