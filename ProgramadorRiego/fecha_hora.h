uint8_t segundos = 0;
uint8_t minutos = 0;
uint8_t horas = 0;

uint8_t fecha_anno = 0; //Año, contando desde el 2000
uint8_t fecha_mes = 1; //Mes, desde 1 hasta 12
uint8_t fecha_dia = 1; //Día del mes, desde 1 hasta 31

uint8_t getSegundos() {	return segundos; }
uint8_t getMinutos() { return minutos; }
uint8_t getHoras() { return horas; }
uint8_t getFecha_Dia() { return fecha_dia; }
uint8_t getFecha_Mes() { return fecha_mes; }
uint8_t getFecha_Anno() { return fecha_anno; }
void setSegundos(uint8_t seg) { segundos = seg; }
void setMinutos(uint8_t min) { minutos = min; }
void setHoras(uint8_t hor) { horas = hor; }
void setDia (uint8_t dia) { fecha_dia = dia; }
void setMes (uint8_t mes) { fecha_mes = mes; }
void setAnno (uint8_t anno) { fecha_anno = anno; }

void actualizarFecha(void);
void actualizarReloj(void);
uint8_t esAnnoBisiesto(uint8_t anno);


void actualizarReloj()
{
	segundos++;
			
	if (segundos == 60) //Sólo aquí, actualizar LCD para actualizar la hora
	{
		segundos = 0;				
		minutos++;
				
		if (minutos == 60)
		{
			minutos = 0;					
			horas++;
					
			if (horas == 24) 
			{
				horas = 0;
				actualizarFecha();
			}			
		}
	}
}

void actualizarFecha()
{
	uint8_t dias_febrero = 28; //Para los años no bisiestos, 28, para los bisiestos, 29
	
	fecha_dia++;
	
	switch(fecha_mes)
	{
		//Meses de 31 días
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			if (fecha_dia > 31)
			{
				fecha_mes++;
				fecha_dia = 1;
			}
			if (fecha_mes > 12)
			{
				fecha_anno++;
				fecha_mes = 1;				
			}
			break;
		//Meses de 30 días	
		case 4:
		case 6:
		case 9:
		case 11:
			if (fecha_dia > 30)
			{
				fecha_mes++;
				fecha_dia = 1;
			}		
			break;	
		case 2:
			if (esAnnoBisiesto(fecha_anno)) dias_febrero = 29;
			
			if (fecha_dia > dias_febrero)
			{
				fecha_mes++;
				fecha_dia = 1;
			}
			
			break;	
		default:
			break;	
			
	}
}

//Un año es bisiesto si:
//- Es múltiplo de 4 y no de 100 ó:
//- Si es múltiplo de 100, debe serlo también de 400.
//Dado que aquí sólo consideramos años entre el 2000 y el 2255, simplificamos tan sólo a que sea múltiplo de 4 y no de 100
uint8_t esAnnoBisiesto(uint8_t anno)
{
	return ((anno%4 == 0) && (anno%100 != 0));
}