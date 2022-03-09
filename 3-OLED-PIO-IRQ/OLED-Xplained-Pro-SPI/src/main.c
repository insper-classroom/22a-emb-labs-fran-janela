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
// Configs leds OLED1

#define LED2_PIO             PIOC            
#define LED2_PIO_ID		     ID_PIOC	  
#define LED2_PIO_IDX         30              
#define LED2_PIO_IDX_MASK    (1 << LED2_PIO_IDX)  


// Configs btns OLED1
#define FREQ_CHANGE_PIO			  PIOD
#define FREQ_CHANGE_PIO_ID        ID_PIOD
#define FREQ_CHANGE_PIO_IDX		  28
#define FREQ_CHANGE_PIO_IDX_MASK  (1u << FREQ_CHANGE_PIO_IDX)

#define START_STOP_PIO			  PIOC
#define START_STOP_PIO_ID         ID_PIOC
#define START_STOP_PIO_IDX		  31
#define START_STOP_PIO_IDX_MASK   (1u << START_STOP_PIO_IDX)

#define DIM_FREQ_PIO			  PIOA
#define DIM_FREQ_PIO_ID           ID_PIOA
#define DIM_FREQ_PIO_IDX		  19
#define DIM_FREQ_PIO_IDX_MASK     (1u << DIM_FREQ_PIO_IDX)


/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

volatile char freq_change_flag;
volatile char start_stop_flag;
volatile char dim_freq_flag;

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void io_init(void);
void piscar(Pio *, const uint32_t, int, int);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

void freq_change_callback(void){
	if (!pio_get(FREQ_CHANGE_PIO, PIO_INPUT, FREQ_CHANGE_PIO_IDX_MASK)) {
	    // PINO == 0 --> Borda de descida
		freq_change_flag = 1;
	} else {
	    // PINO == 1 --> Borda de subida
		freq_change_flag = 0;
    }
}

void start_stop_callback(void){
	start_stop_flag = 1;
}

void dim_freq_callback(void){
	dim_freq_flag = 1;
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

void piscar(Pio *pio, const uint32_t mask, int reps, int freq){
    int cnt = 0;
	gfx_mono_generic_draw_horizontal_line(80, 21, 30, GFX_PIXEL_SET);
    while(cnt<reps*2){
		if(start_stop_flag){
			pio_set(pio, mask);
			start_stop_flag = 0;
			break;
		}
		if(cnt%2 == 0){
			gfx_mono_generic_draw_vertical_line(80+(cnt/2), 12, 10, GFX_PIXEL_SET);
		}
        pin_toggle(pio, mask);
        delay_ms(freq/2);
        cnt++;
    }
	gfx_mono_generic_draw_filled_rect(80, 12, 30, 10, GFX_PIXEL_CLR);
	
}

void oled_freq_update(int freq){
	char freq_str[10];
	gfx_mono_draw_string("          ", 5, 10, &sysfont);
	sprintf(freq_str, "%d ms", freq);
	gfx_mono_draw_string(freq_str, 5, 10, &sysfont);
}

int change_frequency(int freq){
	if(dim_freq_flag){
		dim_freq_flag = 0;
		freq -= 100;
		oled_freq_update(freq);
		return freq;
	}
	for(double i = 0; i<2500000; i++){
		if(!freq_change_flag){
			freq += 100;
			oled_freq_update(freq);
			return freq ;
		}
	}
	freq_change_flag = 0;
	freq -= 100;
	oled_freq_update(freq);
	return freq;
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
    pmc_enable_periph_clk(LED2_PIO_ID);
    pio_set_output(LED2_PIO, LED2_PIO_IDX_MASK, 0, 0, 1);
    
	
	
	// Botão de mudança de frequência:
	pmc_enable_periph_clk(FREQ_CHANGE_PIO_ID);
	pio_configure(FREQ_CHANGE_PIO, PIO_INPUT, FREQ_CHANGE_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(FREQ_CHANGE_PIO, FREQ_CHANGE_PIO_IDX_MASK, 60);
	pio_handler_set(FREQ_CHANGE_PIO,
                  FREQ_CHANGE_PIO_ID,
                  FREQ_CHANGE_PIO_IDX_MASK,
                  PIO_IT_EDGE,
                  freq_change_callback);
    pio_enable_interrupt(FREQ_CHANGE_PIO, FREQ_CHANGE_PIO_IDX_MASK);
    pio_get_interrupt_status(FREQ_CHANGE_PIO);
    NVIC_EnableIRQ(FREQ_CHANGE_PIO_ID);
    NVIC_SetPriority(FREQ_CHANGE_PIO_ID, 4);
	
	// Botão de mudança de frequência:
	pmc_enable_periph_clk(START_STOP_PIO_ID);
	pio_configure(START_STOP_PIO, PIO_INPUT, START_STOP_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(START_STOP_PIO, START_STOP_PIO_IDX_MASK, 60);
	pio_handler_set(START_STOP_PIO,
                  START_STOP_PIO_ID,
                  START_STOP_PIO_IDX_MASK,
                  PIO_IT_FALL_EDGE,
                  start_stop_callback);
    pio_enable_interrupt(START_STOP_PIO, START_STOP_PIO_IDX_MASK);
    pio_get_interrupt_status(START_STOP_PIO);
    NVIC_EnableIRQ(START_STOP_PIO_ID);
    NVIC_SetPriority(START_STOP_PIO_ID, 5);
	
	// Botão de mudança de frequência:
	pmc_enable_periph_clk(DIM_FREQ_PIO_ID);
	pio_configure(DIM_FREQ_PIO, PIO_INPUT, DIM_FREQ_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(DIM_FREQ_PIO, DIM_FREQ_PIO_IDX_MASK, 60);
	pio_handler_set(DIM_FREQ_PIO,
                  DIM_FREQ_PIO_ID,
                  DIM_FREQ_PIO_IDX_MASK,
                  PIO_IT_FALL_EDGE,
                  dim_freq_callback);
    pio_enable_interrupt(DIM_FREQ_PIO, DIM_FREQ_PIO_IDX_MASK);
    pio_get_interrupt_status(DIM_FREQ_PIO);
    NVIC_EnableIRQ(DIM_FREQ_PIO_ID);
    NVIC_SetPriority(DIM_FREQ_PIO_ID, 4);
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
  
	// Init da frequência em ms:
	int freq = 500;

	// Desliga o LED2
    pio_set(LED2_PIO, LED2_PIO_IDX_MASK);


	// Escreve na tela a frequência inicial
	char freq_str[10];
	sprintf(freq_str, "%d ms", freq);
	gfx_mono_draw_string(freq_str, 5, 10, &sysfont);

	/* Insert application code here, after the board has been initialized. */
	while(1) {
		if(freq_change_flag || start_stop_flag || dim_freq_flag){
			if(freq_change_flag){
				freq = change_frequency(freq);
			}
			else if(dim_freq_flag){
				freq = change_frequency(freq);
			}
			else if(start_stop_flag){
				start_stop_flag = 0;
				piscar(LED2_PIO, LED2_PIO_IDX_MASK, 30, freq);
			}
			freq_change_flag = 0;
			start_stop_flag = 0;
			dim_freq_flag = 0;
		}
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); // SLEEP
	}
}
