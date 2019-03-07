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

enum Pantallas {DEBUG, FECHA_HORA, CNT_TIMER, TST_LCD, FECHA_HORA_EDIT};

//Declaración de variables globales
static uint8_t contador_interr = 0;
//Variables relativas a la lectura del teclado
static char tecla = 0x00;
static char tecla0 = 0x00;
volatile static uint8_t fila = 0;
volatile static uint8_t columna = 0;
static uint8_t hay_tecla = 0;
uint8_t bucle_teclado_recorrido = 0;

//Variables relativas a la escritura en el LCD
static uint8_t escribir_lcd = 0; //Escribiremos en el LCD sólo si hay cambios
uint8_t pantalla_activa = TST_LCD;
uint8_t pantalla_activa_previa = TST_LCD;
uint8_t pos_horizontal = 0; //Número de columna
uint8_t pos_horizontal_prev = 0;
uint8_t pos_vertical = 0; //Número de fila
uint8_t pos_vertical_prev = 0;

//Variables para gestión del TIMER
volatile uint16_t contador_timer = 0; //Hasta 63
volatile uint8_t contador = 0; 

//Variables para fecha y hora (de momento, sólo hora)
uint8_t hora;
uint8_t minuto;
uint8_t segundo;
uint8_t fecha_hora_teclas[4] = {0,0,0,0};



//Declaración de funciones
void setup_external_int(void);
void setup_timer0(void);

void actualizar_LCD(void);
void explorar_teclado(void);
void procesar_accion(void);

//Definición de funciones
//Función de configuración de las interrupciones externas INT0 (pin PD2)
void setup_external_int(void)
{
	DDRD &= ~(1 << DDD2);     // Clear the PD2 pin
	EICRA |= (1 << ISC00)|(1 << ISC01);    // set INT0 to trigger on RISING logic change	
	EIMSK |= (1 << INT0);     // Turns on INT0
	sei();                    // turn on interrupts
	
}

//Función de configuración del TIMER0
void setup_timer0(void)
{
	// ******* Configuramos el TIMER 0 para que genere un evento cada 16 ms
	// Set the Timer Mode to CTC
	TCCR0A |= (1 << WGM01);

	//Elegimos un prescaler (en este caso 1024), el OCR0A vale: OCR = (f_clk * T_timer_secs)/prescaler - 1
	//f_clk = 16 MHz, T = 16ms, PS = 1024 nos da un OCR = 255 (redondeado al entero más próximo, ya que la operación da un resultado con decimales)	
	OCR0A = 0xFA; //250
	TIMSK0 |= (1 << OCIE0A);    //Set the ISR COMPA vect
	TCCR0B |= (1 << CS02) | (1 << CS00); //Prescaler 1024	
	sei();
}

void actualizar_LCD()
{
	//Actualizar el teclado
	if (escribir_lcd)
	{
		switch (pantalla_activa)
		{
			case TST_LCD:
				//Lcd4_Clear();
				Lcd4_Init();
				Lcd4_Clear();
				Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
				Lcd4_Write_String("BLINK: ");
				Lcd4_Set_Cursor(1,9);
				_delay_ms(20);
				//En modo 4 bits, los comandos deben enviarse así: primero un 0x00 y luego el comando con los cuatro bits más significativos a cerapio
				Lcd4_Cmd(0x00);
				Lcd4_Cmd(0x0F);
				_delay_ms(20);
				escribir_lcd = 0;
				break;
				
			case DEBUG:
				Lcd4_Clear();
				Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
				Lcd4_Write_String("TECLA: ");
				Lcd4_Set_Cursor(1,9);
				Lcd4_Write_Char(tecla); //Tecla pulsada
				escribir_lcd = 0;
				Lcd4_Set_Cursor(2,1);
				Lcd4_Write_String("PULSADO: ");
				Lcd4_Set_Cursor(2,12);
				Lcd4_Write_Char(hay_tecla?49:48);
				break;
			case FECHA_HORA:			
				if (pantalla_activa_previa != FECHA_HORA) Lcd4_Clear();
				Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
				Lcd4_Write_String("F: DD/MM/AAAA");			
				Lcd4_Set_Cursor(2,1); //Cursor en la segunda línea
				Lcd4_Write_String("H: HH:MM");
				// -- Sólo para probar cursor superpuesto sobre un caracter adicional
				//Lcd4_Set_Cursor(2,4);
				//Lcd4_Cmd(0x00);
				//Lcd4_Cmd(0x0F);
				
				escribir_lcd = 0;
				break;
			case FECHA_HORA_EDIT:	
				if (pantalla_activa_previa != FECHA_HORA_EDIT) 
				{
					Lcd4_Clear();
					Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
					Lcd4_Write_String("F: DD/MM/AAAA");
					Lcd4_Set_Cursor(2,1); //Cursor en la segunda línea
					Lcd4_Write_String("H: HH:MM (E)");
					Lcd4_Set_Cursor_Sts(1,1);
					// -- Sólo para probar cursor superpuesto sobre un caracter adicional
					//Lcd4_Set_Cursor(2,4);
					//Lcd4_Cmd(0x00);
					//Lcd4_Cmd(0x0F);
					//pos_horizontal = 4; //De 0 a 15
					//pos_vertical = 1; //0->primera fila, 1->segunda fila		
					escribir_lcd = 0;					
				}
				else
				{
					Lcd4_Clear();
					Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
					Lcd4_Write_String("F: DD/MM/AAAA");
					Lcd4_Set_Cursor(2,1); //Cursor en la segunda línea
					//Lcd4_Write_String("H: HH:MM (X)");
					Lcd4_Write_String("H: ");
					
					for (int i = 4; i < pos_horizontal; i++)
					{
						Lcd4_Set_Cursor(i, 1);
						Lcd4_Write_Char(fecha_hora_teclas[i-4]);
					}
					
					Lcd4_Set_Cursor(2,pos_horizontal);
					Lcd4_Set_Cursor_Sts(1,1);
					escribir_lcd = 0;
				}
				break;
			case CNT_TIMER:
				if (pantalla_activa_previa != CNT_TIMER) Lcd4_Clear();
				Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
				Lcd4_Write_String("CONTADOR: ");
				Lcd4_Set_Cursor(1,12);
				Lcd4_Write_Char(48+contador);
				escribir_lcd = 0;
				break;
		}
	}	
}

void explorar_teclado()
{
		//Exploración del teclado
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
		
		_delay_ms(5); //Conviene dejar un tiempo desde que excitamos la fila, para que pueda ser leída la columna correspondiente (caso de pulsarse una tecla).
		
		//Detectar tecla pulsada
		if (((PINB & 0b00111100) != 0b00111100) /*&& (!hay_tecla)*/)
		{
			//Hacemos el barrido por todas ellas hasta que encontremos la fila activa
			fila = 0;
			columna = 0;	
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
			}
		}
		else
		{
			tecla = 'N';			
			hay_tecla = 0;			
		}
		
		fila = (fila + 1)%4;	
}

void procesar_accion()
{
	/*if (pantalla_activa == TST_LCD)
	{
		if (tecla != 'N')
		{
			if (pos_horizontal <= 15) 
			{
				pos_horizontal++;	
			}			
			else
			{
				if (pos_vertical == 0) pos_vertical = 1;
				else pos_vertical = 0;
				
				pos_horizontal = 0;				
			}
		}
		else if (tecla == 'D') pantalla_activa = DEBUG;
	}	
	else*/ if (tecla == 'D') pantalla_activa = DEBUG;
	else if (tecla == 'A') pantalla_activa = FECHA_HORA;
	else if (tecla == 'C') pantalla_activa = CNT_TIMER;
	else 
	{			
		if (pantalla_activa == FECHA_HORA)
		{
			if (tecla == 'B') 
			{
				pantalla_activa = FECHA_HORA_EDIT;	
				escribir_lcd = 1;	
			}			
		}
		else if (pantalla_activa == FECHA_HORA_EDIT) //Aquí, editar fecha y hora
		{
			if (pantalla_activa_previa == FECHA_HORA) 
			{
				//Lcd4_Set_Cursor(2,4);	
				//Lcd4_Cmd(0x00);
				//Lcd4_Cmd(0x0F);
				pos_horizontal = 0; //De 0 a 15
				pos_vertical = 1; //0->primera fila, 1->segunda fila
				escribir_lcd = 1;
			}
			else
			{
				if ((tecla >= 48) && (tecla <= 57)) //Sólo teclas numéricas
				{
					if (pos_horizontal == 0) pos_horizontal = 4;
					else if (tecla != tecla0) pos_horizontal++;
					
					switch (pos_horizontal)
					{
						case 4: 
							fecha_hora_teclas[0] = tecla;
							break;
						case 5:	
							fecha_hora_teclas[1] = tecla;
							break;						
						case 7:
							fecha_hora_teclas[2] = tecla;
							break;
						case 8:
							fecha_hora_teclas[3] = tecla;
							break;							
						default: 
							break;
					}
					
					
					//if (pos_horizontal <= 9) pos_horizontal++;
					//if (pos_horizontal == 6) pos_horizontal++; //Para saltarnos el carácter ':'
					//Lcd4_Set_Cursor(2,pos_horizontal);
					escribir_lcd = 1;
				}				
			}			
		}
	}
	/* //Descomentar sólo para pruebas y/o debug
	else if (tecla == '*') Lcd4_Set_Cursor_Sts(1,0);
	else if (tecla == '#') Lcd4_Set_Cursor_Sts(1,1);
	else if (tecla == '0') Lcd4_Set_Cursor_Sts(0,0);
	*/
	
}


int main(void)
{
	DDRD = 0xFF;
	DDRC = 0x0F;

	//Configurar los pines PB2, PB3, PB4 y PB5 como entradas, PB0 y PB1 como salidas
	DDRB &= ~((1 << DDB2) | (1 << DDB3) | (1 << DDB4) | (1 << DDB5));
	DDRB |= ((1 << DDB0) | (1 << DDB1));
	
	//Activar resistencias  de PULL UP en PB2, PB3, PB4, PB5
	PORTB |= ((1 << PORTB2) | (1 << PORTB3) | (1 << PORTB4) | (1 << PORTB5));
	
	//Inicializamos el puerto C (salidas conectadas a las filas del teclado matricial)
	PORTC = (1 << PORTC0) | (1 << PORTC1) | (1 << PORTC2) | (1 << PORTC3);
	
	//setup_external_int();
	setup_timer0();
		
	//Escribimos una primera vez en el LCD, luego sólo si hay cambios
	escribir_lcd = 1;

	Lcd4_Init();
	Lcd4_Clear();
	
	while(1)
	{
		explorar_teclado();
		if (pantalla_activa == DEBUG)
		{		
			if (tecla != tecla0) escribir_lcd = 1;
			else escribir_lcd = 0;		
		}
		else if (pantalla_activa == FECHA_HORA) 
		{
			//escribir_lcd = 1;	
		}
		else if (pantalla_activa == TST_LCD)
		{
			if ((pos_vertical_prev != pos_vertical)	|| (pos_horizontal_prev != pos_horizontal))
			{
				escribir_lcd = 1;				
			}
			
			pos_vertical_prev = pos_vertical;
			pos_horizontal_prev = pos_horizontal;
		}
		
		procesar_accion();
		actualizar_LCD();		
		tecla0 = tecla;
		pantalla_activa_previa = pantalla_activa;
		_delay_ms(10);
	}
}

//FUNCIÓN QUE SE LLAMARÁ CON CADA INTERRUPCIÓN EXTERNA DE FLANCO
ISR (INT0_vect)
{
	contador_interr++;
}

//FUNCION QUE SE LLAMARÁ CON CADA EVENTO DEL TIMER
ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{	
	contador_timer++;

	if (contador_timer == 62)
	{
		contador_timer = 0; //1 segundo son 62,5 pulsos de 16 ms cada uno
		contador++;	
		
		if (contador == 10) contador = 0;
		if (pantalla_activa == CNT_TIMER) escribir_lcd = 1;
	}	
}