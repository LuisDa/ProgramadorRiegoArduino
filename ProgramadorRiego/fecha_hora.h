uint8_t segundos = 0;
uint8_t minutos = 0;
uint8_t horas = 0;

uint8_t getSegundos() {	return segundos; }
uint8_t getMinutos() { return minutos; }
uint8_t getHoras() { return horas; }
void setSegundos(uint8_t seg) { segundos = seg; }
void setMinutos(uint8_t min) { minutos = min; }
void setHoras(uint8_t hor) { horas = hor; }

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
					
			if (horas == 24) horas = 0;
		}
	}
}