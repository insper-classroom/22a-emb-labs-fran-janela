/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"


/************************************************************************/
/* defines                                                              */
/************************************************************************/

// Configs btns OLED1
#define CALL_PIO			  PIOD
#define CALL_PIO_ID        ID_PIOD
#define CALL_PIO_IDX		  28
#define CALL_PIO_IDX_MASK  (1u << CALL_PIO_IDX)


// Configs Ultrassom
#define TRIG_PIO			  PIOA
#define TRIG_PIO_ID        ID_PIOA
#define TRIG_PIO_IDX		  24
#define TRIG_PIO_IDX_MASK  (1u << TRIG_PIO_IDX)

#define ECHO_PIO			  PIOD
#define ECHO_PIO_ID        ID_PIOD
#define ECHO_PIO_IDX		  26
#define ECHO_PIO_IDX_MASK  (1u << ECHO_PIO_IDX)


/************************************************************************/
/* constants                                                            */
/************************************************************************/

int freq = (float)1/(0.000058*2);
double pulses;


/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

volatile char call_flag = 0;
volatile char echo_flag = 0;
volatile char erro_flag = 0;

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void io_init(void);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}


void call_callback(void){
	call_flag = 1;
}

void echo_callback(void){
	if (pio_get(ECHO_PIO, PIO_INPUT, ECHO_PIO_IDX_MASK)) {
		// PINO == 1 --> Borda de descida
		if (call_flag){
			/*RTT_init(freq, ((float)4/340)*freq*2 , RTT_MR_ALMIEN);*/
			RTT_init(freq, 0 , 0);
			echo_flag = 1;
		} else {
			erro_flag = 1;
		}
	} else {
		// PINO == 0 --> Borda de subida
		echo_flag = 0;
		pulses = rtt_read_timer_value(RTT);
	}
}

void RTT_Handler(void) {
	uint32_t ul_status;

	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		erro_flag = 1;
	}
	
	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
	}
}


/************************************************************************/
/* funcoes                                                              */
/************************************************************************/


void pin_toggle(Pio *pio, uint32_t mask) {
	if(pio_get_output_data_status(pio, mask))
		pio_clear(pio, mask);
	else
		pio_set(pio,mask);
}

void piscar_trig(){
    pio_clear(TRIG_PIO, TRIG_PIO_IDX_MASK);
	delay_us(10);
 	pio_set(TRIG_PIO, TRIG_PIO_IDX_MASK);

}

float ler_distancia(){
	while(echo_flag){
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); // SLEEP
	}
	float distancia = (float)(100.0*pulses*340)/(freq*2);
	if(distancia > 400){
		return -1;
	}
	return distancia;
}

/************************************************************************/
/* init                                                              */
/************************************************************************/

// Função de inicialização do uC
void io_init(void){

	// Initialize the board clock
	sysclk_init();

	// Desativa WatchDog Timer
	WDT->WDT_MR = WDT_MR_WDDIS;


    // Leds OLED1
    pmc_enable_periph_clk(TRIG_PIO_ID);
    pio_set_output(TRIG_PIO, TRIG_PIO_IDX_MASK, 0, 0, 1);
    
	
	
	// Botão para chamar o trigger:
	pmc_enable_periph_clk(CALL_PIO_ID);
	pio_configure(CALL_PIO, PIO_INPUT, CALL_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(CALL_PIO, CALL_PIO_IDX_MASK, 60);
	pio_handler_set(CALL_PIO,
                  CALL_PIO_ID,
                  CALL_PIO_IDX_MASK,
                  PIO_IT_FALL_EDGE,
                  call_callback);
    pio_enable_interrupt(CALL_PIO, CALL_PIO_IDX_MASK);
    pio_get_interrupt_status(CALL_PIO);
    NVIC_EnableIRQ(CALL_PIO_ID);
    NVIC_SetPriority(CALL_PIO_ID, 4);
	
	// Leitura do Echo:
	pmc_enable_periph_clk(ECHO_PIO_ID);
	pio_configure(ECHO_PIO, PIO_INPUT,ECHO_PIO_IDX_MASK, PIO_DEFAULT);
	pio_set_debounce_filter(ECHO_PIO, ECHO_PIO_IDX_MASK, 60);
	pio_handler_set(ECHO_PIO,
                  ECHO_PIO_ID,
                  ECHO_PIO_IDX_MASK,
                  PIO_IT_EDGE,
                  echo_callback);
    pio_enable_interrupt(ECHO_PIO, ECHO_PIO_IDX_MASK);
    pio_get_interrupt_status(ECHO_PIO);
    NVIC_EnableIRQ(ECHO_PIO_ID);
    NVIC_SetPriority(ECHO_PIO_ID, 5);
}



/************************************************************************/
/* Main                                                                 */
/************************************************************************/

int main (void)
{
	board_init();
	delay_init();

	// IO Init:
	io_init();

	// Init OLED
	gfx_mono_ssd1306_init();
  

	/* Insert application code here, after the board has been initialized. */
	while(1) {
		if (erro_flag){
			gfx_mono_draw_string("ERROR     ", 5, 10, &sysfont);
			erro_flag = 0;
		} else if(echo_flag){
			int distancia = ler_distancia();
			if(distancia != -1){
				char dist_str[10];
				sprintf(dist_str, "%d cm    ", distancia);
				gfx_mono_draw_string(dist_str, 5, 10, &sysfont);
			} else {
				gfx_mono_draw_string("ERROR DIS ", 5, 10, &sysfont);
				erro_flag = 0;
			}
			call_flag = 0;
		} else if(call_flag){
			piscar_trig();
		}
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); // SLEEP
	}
}
