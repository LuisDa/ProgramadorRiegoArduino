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
#include "lcd.h" 
#include "fecha_hora.h"

static const char getKeyPressed[4][4] = {{'1', '2', '3', 'A'},
										 {'4', '5', '6', 'B'},
										 {'7', '8', '9', 'C'}, 
										 {'*', '0', '#', 'D'}};

enum Pantallas {DEBUG, FECHA_HORA, CNT_TIMER, TST_LCD, FECHA_HORA_EDIT, MENU_PROGRAMAS, TEMPERATURA};
enum OpcionesMenu {PROGRAMAS_RIEGO=0, ACTIVAR_MANUAL=1, CONDIC_AMBIENTE=2};

//Declaración de variables globales
static uint8_t contador_interr = 0;
//Variables relativas a la lectura del teclado
static char tecla = 0x00;
static char tecla0 = 0x00;
volatile static uint8_t fila = 0;
volatile static uint8_t columna = 0;
static uint8_t hay_tecla = 0;
uint8_t bucle_teclado_recorrido = 0;
uint8_t hora_introducida = 0;
uint8_t fecha_introducida = 0;
static volatile uint16_t lectura_ADC = 0;
float voltajeADC_mV = 0.0;
uint8_t temperatura = 0;
uint8_t temperatura_previa = 0;

//Variables relativas a la escritura en el LCD
static uint8_t escribir_lcd = 0; //Escribiremos en el LCD sólo si hay cambios
uint8_t pantalla_activa = FECHA_HORA;
uint8_t pantalla_activa_previa = FECHA_HORA;
//uint8_t pantalla_activa = TST_LCD;
//uint8_t pantalla_activa_previa = TST_LCD;
//uint8_t pantalla_activa = FECHA_HORA_EDIT;
//uint8_t pantalla_activa_previa = FECHA_HORA;
uint8_t pos_horizontal = 0; //Número de columna
uint8_t pos_horizontal_prev = 0;
uint8_t pos_vertical = 0; //Número de fila
uint8_t pos_vertical_prev = 0;

uint8_t pos_menu_actual = PROGRAMAS_RIEGO;

//Variables para gestión del TIMER
volatile uint16_t contador_timer = 0; //Hasta 63
volatile uint8_t contador = 0; 

//Variables para fecha y hora (de momento, sólo hora)
uint8_t hora = 0;
uint8_t minuto = 0;
uint8_t segundo = 0;
uint8_t anno = 0; //Contando desde el año 2000
uint8_t mes = 1;
uint8_t dia = 1;
uint8_t fecha_hora_teclas[4] = {0,0,0,0};
uint8_t fecha_hora_caracteres[5] = {'_', '_', ':', '_', '_'};
uint8_t fecha_caracteres[10] = {'D', 'D', '/', 'M', 'M', '/', '2', 'A', 'A', 'A'};



//Declaración de funciones
void setup_external_int(void);
void setup_timer0(void);
void setup_ADC(void);

uint16_t ReadADC(uint8_t ADC_Channel);

void actualizar_LCD(void);
void explorar_teclado(void);
void procesar_accion(void);

void imprimir_fecha(void);

//Definición de funciones
//Función de configuración del ADC (sólo podremos, en este proyecto, usar las entradas 4 y 5, las otras cuatro están pilladas para lectura del teclado)
void setup_ADC(void)
{
	// Select Vref=AVcc
	ADMUX |= (1<<REFS0);
	//set prescaller to 128 and enable ADC
	ADCSRA |= (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADEN);	
}

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

//Función de lectura del ADC 
uint16_t ReadADC(uint8_t ADC_Channel)
{
	//select ADC channel with safety mask
	ADMUX = (ADMUX & 0xF0) | (ADC_Channel & 0x0F);
	//single conversion mode
	ADCSRA |= (1<<ADSC);
	// wait until ADC conversion is complete
	while( ADCSRA & (1<<ADSC) );
	return ADC;
}

void actualizar_LCD()
{
	//Actualizar el teclado
	if (escribir_lcd)
	{
		switch (pantalla_activa)
		{
			case MENU_PROGRAMAS:
				Lcd4_Clear();
				Lcd4_Set_Cursor(1,0);
				Lcd4_Write_String("MENU: ");
				Lcd4_Set_Cursor(2,0);
				if (pos_menu_actual == PROGRAMAS_RIEGO) Lcd4_Write_String("1- PROG. RIEGO");
				else if (pos_menu_actual == ACTIVAR_MANUAL) Lcd4_Write_String("2- ACTIV. MANUAL");
				else if (pos_menu_actual == CONDIC_AMBIENTE) Lcd4_Write_String("3- COND. AMBIENTE");
				escribir_lcd = 0;
				break;
			case TEMPERATURA:
				Lcd4_Clear();
				Lcd4_Set_Cursor(1,0);
				Lcd4_Write_String("TEMP: ");
				uint8_t temp_decenas = (temperatura/10) + 48;
				uint8_t temp_unidades = (temperatura%10) + 48;
				Lcd4_Write_Char(temp_decenas);
				Lcd4_Write_Char(temp_unidades);
				Lcd4_Write_String(" ºC");
				escribir_lcd = 0;
				//Lcd4_Write_String();
				break;			
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
				
				//Pintamos la fecha
				//Lcd4_Write_String("F: DD/MM/AAAA");	
				/*Lcd4_Write_String("F: ");
				Lcd4_Write_Char(getFecha_Dia()/10 + 48);
				Lcd4_Write_Char(getFecha_Dia()%10 + 48);
				Lcd4_Write_Char('/');
				Lcd4_Write_Char(getFecha_Mes()/10 + 48);
				Lcd4_Write_Char(getFecha_Mes()%10 + 48);
				Lcd4_Write_Char('/');
				Lcd4_Write_Char(50); //Cifra '2', de los millares
				Lcd4_Write_Char(getFecha_Anno()/100 + 48); //La variable 'anno' va de 0 a 255. Centenas
				Lcd4_Write_Char(((getFecha_Anno()/10)%10) + 48); //Decenas
				Lcd4_Write_Char((getFecha_Anno()%10) + 48); //Unidades*/
				imprimir_fecha();
						
				//Pintamos la hora		
				Lcd4_Set_Cursor(2,1); //Cursor en la segunda línea				
				Lcd4_Write_String("H: ");
				
				uint8_t hora_decenas = (hora/10) + 48;
				uint8_t hora_unidades = (hora%10) + 48;
				uint8_t minuto_decenas = (minuto/10) + 48;
				uint8_t minuto_unidades = (minuto%10) + 48;
				uint8_t segundo_decenas = (segundo/10) + 48;
				uint8_t segundo_unidades = (segundo%10) + 48;
					
				Lcd4_Write_Char(hora_decenas);
				Lcd4_Write_Char(hora_unidades);
				Lcd4_Write_Char(':');
				Lcd4_Write_Char(minuto_decenas);
				Lcd4_Write_Char(minuto_unidades);
				Lcd4_Write_Char(':');
				Lcd4_Write_Char(segundo_decenas);
				Lcd4_Write_Char(segundo_unidades);
								
				Lcd4_Set_Cursor_Sts(0,0);
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
					//Lcd4_Write_String("F: DD/MM/AAAA");
					if (fecha_introducida == 0)
					{
						Lcd4_Write_String("F: ");
						
						for(int i = 0; i <= 9; i++)
						{
							Lcd4_Write_Char(fecha_caracteres[i]);
						}
					}
					else
					{
						imprimir_fecha();
					}
					Lcd4_Set_Cursor(2,1); //Cursor en la segunda línea	
					if (hora_introducida == 0)
					{
						Lcd4_Write_String("H: __:__");		
					}					
					else
					{
						Lcd4_Write_String("H: ");
						
						//for(int i = 0; i <= 4; i++)
						//{
							//Lcd4_Write_Char(fecha_hora_caracteres[i]);
							uint8_t hora_decenas = (hora/10) + 48;
							uint8_t hora_unidades = (hora%10) + 48;
							uint8_t minuto_decenas = (minuto/10) + 48;
							uint8_t minuto_unidades = (minuto%10) + 48;
							Lcd4_Write_Char(hora_decenas);
							Lcd4_Write_Char(hora_unidades);
							Lcd4_Write_Char(':');
							Lcd4_Write_Char(minuto_decenas);
							Lcd4_Write_Char(minuto_unidades);
						//}												
					}
					
					//Lcd4_Set_Cursor(2,4);				
					Lcd4_Set_Cursor((pos_vertical+1), 4);
					Lcd4_Set_Cursor_Sts(1,1);	
					escribir_lcd = 0;					
				}
				else
				{
					Lcd4_Set_Cursor(1,1); //Cursor en la primera línea
					//Lcd4_Write_String("F: DD/MM/AAAA");
					Lcd4_Write_String("F: ");
					for(int i = 0; i <= 9; i++)
					{
						Lcd4_Write_Char(fecha_caracteres[i]);
					}
					
					Lcd4_Set_Cursor(2,4);
					
					for(int i = 0; i <= 4; i++)
					{						
						Lcd4_Write_Char(fecha_hora_caracteres[i]);
					}
										
					//Lcd4_Set_Cursor(2,pos_horizontal);
					//Lcd4_Set_Cursor_Sts(1,1);
					
					if (pos_vertical == 0) Lcd4_Set_Cursor(1,pos_horizontal);
					else Lcd4_Set_Cursor(2,pos_horizontal);
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
	if ((tecla == 'D') && (pantalla_activa != FECHA_HORA_EDIT) && (pantalla_activa != MENU_PROGRAMAS))
	{
		pantalla_activa = DEBUG;
		//escribir_lcd = 1;
	}
	else if ((tecla == 'A') && (pantalla_activa != FECHA_HORA_EDIT) && (pantalla_activa != MENU_PROGRAMAS))
	{
		pantalla_activa = FECHA_HORA;
		escribir_lcd = 1;
	}
	else if (tecla == 'C')
	{
		if (pantalla_activa_previa == DEBUG)
		{		
			pantalla_activa = CNT_TIMER;	
			escribir_lcd = 1;
		}
		else
		{
			pantalla_activa = MENU_PROGRAMAS;
			escribir_lcd = 1;
		}
		
		//Si no, aquí entraríamos para editar programas de riego
	}	
	else 
	{			
		if (pantalla_activa == MENU_PROGRAMAS)
		{			
			if (tecla == 'A') 
			{
				//pos_menu_actual--;
				if (pos_menu_actual > 0) pos_menu_actual--;
				else pos_menu_actual = PROGRAMAS_RIEGO;
				escribir_lcd = 1;	
			}
			else if (tecla == 'D')
			{
				//pos_menu_actual++;
				if (pos_menu_actual < 2) pos_menu_actual++;
				else pos_menu_actual = CONDIC_AMBIENTE;
				escribir_lcd = 1;				
			}
			else if ((tecla == '#') && (pos_menu_actual == CONDIC_AMBIENTE))
			{
				pantalla_activa = TEMPERATURA;
				escribir_lcd = 1;
			}
			else if (tecla == '*')
			{
				pantalla_activa = FECHA_HORA;
				escribir_lcd = 1;
			}			
		}	
		else if (pantalla_activa == TEMPERATURA)
		{
			if (tecla == '*')
			{
				pantalla_activa = MENU_PROGRAMAS;
				escribir_lcd = 1;
			}
		}
		else if (pantalla_activa == FECHA_HORA)
		{
			if (tecla == 'B') 
			{
				pantalla_activa = FECHA_HORA_EDIT;	
				escribir_lcd = 1;	
				pos_horizontal = 0;
				pos_vertical = 0;
			}			
		}
		else if (pantalla_activa == FECHA_HORA_EDIT) //Aquí, editar fecha y hora
		{
			if (pantalla_activa_previa == FECHA_HORA) //Parece que en este bloque no se mete nunca...
			{
				pos_horizontal = 0; //De 0 a 15
				pos_vertical = 1; //0->primera fila, 1->segunda fila
				escribir_lcd = 1;
			}
			else if (tecla != tecla0)
			{
				if ((tecla == 'A') && (pantalla_activa == FECHA_HORA_EDIT))
				{
					escribir_lcd = 1;
					pos_vertical = 0;
				}
				else if ((tecla == 'D') && (pantalla_activa_previa == FECHA_HORA_EDIT))
				{
					escribir_lcd = 1;
					pos_vertical = 1;
					pos_horizontal = 4;
				}
				
				if ((tecla >= 48) && (tecla <= 57)) //Sólo teclas numéricas
				{
					if (pos_horizontal == 0) pos_horizontal = 4;
					
					if (pos_vertical == 0)
					{
						switch (pos_horizontal) //Editar fecha
						{
							case 4:
								fecha_caracteres[0] = tecla;
								break;
							case 5:
								fecha_caracteres[1] = tecla;
								break;
							case 7:
								fecha_caracteres[3] = tecla;
								break;
							case 8:
								fecha_caracteres[4] = tecla;
								break;
							case 11: //Pasamos a editar las centenas del año (variará de 2000 a 2255).
								fecha_caracteres[7] = tecla;
								break;
							case 12:
								fecha_caracteres[8] = tecla;
								break;
							case 13:
								fecha_caracteres[9] = tecla;
								break;
						}
						
						if (fecha_introducida == 0) fecha_introducida = 1;
						
						if (pos_horizontal <= 13) pos_horizontal++;
						if (pos_horizontal == 6) pos_horizontal++;
						if (pos_horizontal == 9) pos_horizontal = 11;
						
						//if ((pos_horizontal == 6) || (pos_horizontal == 9) || (pos_horizontal == 10)) pos_horizontal++;
					}
					else if (pos_vertical == 1) //Editar hora
					{					
						switch (pos_horizontal)
						{
							case 4: 
								//fecha_hora_teclas[0] = tecla;
								fecha_hora_caracteres[0] = tecla;
								break;
							case 5:	
								//fecha_hora_teclas[1] = tecla;
								fecha_hora_caracteres[1] = tecla;
								break;						
							case 7:
								//fecha_hora_teclas[2] = tecla;
								fecha_hora_caracteres[3] = tecla;
								break;
							case 8:
								//fecha_hora_teclas[3] = tecla;
								fecha_hora_caracteres[4] = tecla;
								//hora_introducida = 1;
								break;							
							default: 
								break;
						}
					
						if (pos_horizontal <= 8) pos_horizontal++;
						if (pos_horizontal == 6) pos_horizontal++;
					}

					escribir_lcd = 1;
				}
				else if (tecla == '#')
				{
					if (pos_vertical == 0)
					{
						dia = 10*(fecha_caracteres[0] - 48) + (fecha_caracteres[1] - 48);
						mes = 10*(fecha_caracteres[3] - 48) + (fecha_caracteres[4] - 48);
						anno = 100*(fecha_caracteres[7] - 48) + 10*(fecha_caracteres[8] - 48) + (fecha_caracteres[9] - 48);
						
						setDia(dia);
						setMes(mes);
						setAnno(anno);
						
						fecha_introducida = 1;
						pantalla_activa = FECHA_HORA;
						pos_vertical = 1;
						pos_horizontal = 4;
						escribir_lcd = 1;
					}
					else
					{
						minuto = 10*(fecha_hora_caracteres[3] - 48) + (fecha_hora_caracteres[4] - 48);
						hora = 10*(fecha_hora_caracteres[0] - 48) + (fecha_hora_caracteres[1] - 48);
										
						setSegundos(0);
						setMinutos(minuto);
						setHoras(hora);
										
						hora_introducida = 1;
						pantalla_activa = FECHA_HORA;
						escribir_lcd = 1;	
					}
				}
				else if (tecla == '*')
				{
					switch (pos_horizontal)
					{
						case 4:
							fecha_hora_caracteres[0] = '_';
							break;
						case 5:						
							fecha_hora_caracteres[1] = '_';
							break;
						case 7:						
							fecha_hora_caracteres[3] = '_';
							break;
						case 8:						
							fecha_hora_caracteres[4] = '_';						
							break;
						default:
							break;
					}	
					
					if (pos_horizontal > 4) pos_horizontal--;
					if (pos_horizontal == 6) pos_horizontal--;

					escribir_lcd = 1;			
				}
			}			
		}
	}	
}

void imprimir_fecha(void)
{
	Lcd4_Write_String("F: ");
	Lcd4_Write_Char(getFecha_Dia()/10 + 48);
	Lcd4_Write_Char(getFecha_Dia()%10 + 48);
	Lcd4_Write_Char('/');
	Lcd4_Write_Char(getFecha_Mes()/10 + 48);
	Lcd4_Write_Char(getFecha_Mes()%10 + 48);
	Lcd4_Write_Char('/');
	Lcd4_Write_Char(50); //Cifra '2', de los millares
	Lcd4_Write_Char(getFecha_Anno()/100 + 48); //La variable 'anno' va de 0 a 255. Centenas
	Lcd4_Write_Char(((getFecha_Anno()/10)%10) + 48); //Decenas
	Lcd4_Write_Char((getFecha_Anno()%10) + 48); //Unidades
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
	setup_ADC();
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
		


		if (contador_timer == 31)
		{
			lectura_ADC = ReadADC(4);
			//voltajeADC_mV = 5000*lectura_ADC/1023;
			temperatura = (uint8_t)((500*lectura_ADC/1023) - 50);
		}		
		
		if ((temperatura != temperatura_previa) && (pantalla_activa == TEMPERATURA)) escribir_lcd = 1;
		
		procesar_accion();
		actualizar_LCD();		
		tecla0 = tecla;
		pantalla_activa_previa = pantalla_activa;
		temperatura_previa = temperatura;
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
		
		//Actualizar variables del reloj
		
		actualizarReloj();
		
		segundo = getSegundos();
		minuto = getMinutos();
		hora = getHoras();
		
		
		//segundo++;
		/*
		if (segundo == 60) //Sólo aquí, actualizar LCD para actualizar la hora
		{
			segundo = 0;
			
			minuto++;
			
			if (minuto == 60)
			{
				minuto = 0;
				
				hora++;
				
				if (hora == 24) hora = 0;
			}
			
			//if (pantalla_activa == FECHA_HORA) escribir_lcd = 1;
		}
		*/
		
		if (pantalla_activa == FECHA_HORA) escribir_lcd = 1;
		
	}	
	//else if (contador_timer == 31)
	//{
	//	lectura_ADC = ReadADC(4);
	//}
}