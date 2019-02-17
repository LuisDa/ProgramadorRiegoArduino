//Fuente original: https://electrosome.com/interfacing-lcd-atmega32-microcontroller-atmel-studio/

#ifndef F_CPU
#define F_CPU 16000000UL // 16 MHz clock speed
#endif
#define D4 eS_PORTD4
#define D5 eS_PORTD5
#define D6 eS_PORTD6
#define D7 eS_PORTD7
#define RS eS_PORTB0//eS_PORTC6
#define EN eS_PORTB1//eS_PORTC7

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lcd.h" //Can be download from the bottom of this article

static const char getKeyPressed[4][4] = {{'1', '2', '3', 'A'},
										 {'4', '5', '6', 'B'},
										 {'7', '8', '9', 'C'}, 
										 {'*', '0', '#', 'D'}};

//Declaraci�n de variables globales
static uint8_t contador_interr = 0;
//static uint8_t lectura_teclado = 0b00000000;
static char tecla = 0x00;
volatile static uint8_t fila = 0;
volatile static uint8_t columna = 0;
static uint8_t hay_tecla = 0;
static uint8_t escribir_lcd = 0; //Escribiremos en el LCD s�lo si hay cambios
//static uint8_t leyendo_teclado = 0;

//Declaraci�n de funciones
void setup_external_int(void);
void setup_timer0(void);

void actualizar_LCD(void);

//Definici�n de funciones
//Funci�n de configuraci�n de las interrupciones externas INT0 (pin PD2)
void setup_external_int(void)
{
	DDRD &= ~(1 << DDD2);     // Clear the PD2 pin
	// PD2 (PCINT0 pin) is now an input

	//PORTD |= (1 << PORTD2);    // turn On the Pull-up
	// PD2 is now an input with pull-up enabled



	EICRA |= (1 << ISC00)|(1 << ISC01);    // set INT0 to trigger on RISING logic change
	//EICRA |= (1 << ISC01);    // set INT0 to trigger on FALLING logic change
	EIMSK |= (1 << INT0);     // Turns on INT0

	sei();                    // turn on interrupts
	
}

//Funci�n de configuraci�n del TIMER0
void setup_timer0(void)
{
	// ******* Configuramos el TIMER 0 para que genere un evento cada 16 ms
	// Set the Timer Mode to CTC
	TCCR0A |= (1 << WGM01);

	// Set the value that you want to count to
	//OCR0A = 0x9C;//0xF9;
	//Elegimos un prescaler (en este caso 1024), el OCR0A vale: OCR = (f_clk * T_timer_secs)/prescaler - 1
	//f_clk = 16 MHz, T = 16ms, PS = 1024 nos da un OCR = 255 (redondeado al entero m�s pr�ximo, ya que la operaci�n da un resultado con decimales)
	//OCR0A = 0x7D;
	OCR0A = 0xFF;

	TIMSK0 |= (1 << OCIE0A);    //Set the ISR COMPA vect

	sei();         //enable interrupts

	//TCCR0B |= (1 << CS02); //Prescaler 256
	TCCR0B |= (1 << CS02) | (1 << CS00); //Prescaler 1024
}

void actualizar_LCD()
{
	//Actualizar el teclado
	if (escribir_lcd)
	{
		Lcd4_Set_Cursor(1,1); //Cursor en la primera l�nea
		Lcd4_Write_String("TECLA: ");
		Lcd4_Set_Cursor(1,9);
		Lcd4_Write_Char(tecla); //Tecla pulsada
		escribir_lcd = 0;
		Lcd4_Set_Cursor(2,1);
		Lcd4_Write_String("PULSADO: ");
		Lcd4_Set_Cursor(2,12);
		Lcd4_Write_Char(hay_tecla?49:48);
	}	
}

int main(void)
{
	uint8_t bucle_teclado_recorrido = 0;
	
	DDRD = 0xFF;
	DDRC = 0x0F;

	//Configurar los pines PB2, PB3, PB4 y PB5 como entradas, PB0 y PB1 como salidas
	DDRB &= ~((1 << DDB2) | (1 << DDB3) | (1 << DDB4) | (1 << DDB5));
	DDRB |= ((1 << DDB0) | (1 << DDB1));
	
	//Activar resistencias  de PULL UP en PB2, PB3, PB4, PB5
	PORTB |= ((1 << PORTB2) | (1 << PORTB3) | (1 << PORTB4) | (1 << PORTB5));
	
	//Inicializamos el puerto C (salidas conectadas a las filas del teclado matricial)
	PORTC = (1 << PORTC0) | (1 << PORTC1) | (1 << PORTC2) | (1 << PORTC3);
	
	setup_external_int();
		
	//Escribimos una primera vez en el LCD, luego s�lo si hay cambios
	escribir_lcd = 1;

	Lcd4_Init();
	Lcd4_Clear();
	while(1)
	{			
		
		//Exploraci�n del teclado
		if (!hay_tecla)		
		{
			switch(fila)
			{
				case 0: PORTC = 0b00000111; break;
				case 1: PORTC = 0b00001011; break;
				case 2: PORTC = 0b00001101; break;
				case 3: PORTC = 0b00001110; break;
			}
		}
		
		//Actualizar el teclado
		actualizar_LCD();
		
		//Detectar tecla pulsada		
		if (((PINB & 0b00111100) != 0b00111100) /*&& (!hay_tecla)*/)
		{		
			//Hacemos el barrido por todas ellas hasta que encontremos la fila activa
			fila = 0;
			columna = 0;
			//hay_tecla = 0;
			bucle_teclado_recorrido = 0;
		
			//Y buscamos la fila cuya tecla se ha pulsado
			while((!hay_tecla) && (fila <= 3))
			{			
				if (fila == 0) PORTC = 0b00000111;
				else if (fila == 1) PORTC = 0b00001011;
				else if (fila == 2) PORTC = 0b00001101;
				else if (fila == 3) PORTC = 0b00001110;
				
				bucle_teclado_recorrido = 1;
						
				//Esperamos un poco
				_delay_ms(5);
			
				if ((PINB & 0b00111100) == 0b00011100)
				{
					columna = 3;
					hay_tecla = 1;
				}			
				else if ((PINB & 0b00111100) == 0b00101100)
				{				
					columna = 2;
					hay_tecla = 1;
				}
				else if ((PINB & 0b00111100) == 0b00110100)
				{
					columna = 1;
					hay_tecla = 1;				
				}
				else if ((PINB & 0b00111100) == 0b00111000)
				{
					columna = 0;
					hay_tecla = 1;	
				}
				else
				{
					fila++;				
				}
			}

			if (hay_tecla && bucle_teclado_recorrido) 
			{
				tecla = getKeyPressed[fila][columna];
				escribir_lcd = 1;
			}		
		}				
		else
		{		
			tecla = 'N';		
			hay_tecla = 0;
			escribir_lcd = 1;
		}
		
		fila = (fila + 1)%4;
	
		_delay_ms(10);

	}
}

//FUNCI�N QUE SE LLAMAR� CON CADA INTERRUPCI�N EXTERNA DE FLANCO
ISR (INT0_vect)
{
	contador_interr++;
}

//FUNCION QUE SE LLAMAR� CON CADA EVENTO DEL TIMER
ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{
	//fila = (fila + 1)%4;
	
/*
	switch(fila)
	{
		case 0: PORTC = 0b00000111; break;
		case 1: PORTC = 0b00001011; break;
		case 2: PORTC = 0b00001101; break;
		case 3: PORTC = 0b00001110; break;
	}		
*/	
}