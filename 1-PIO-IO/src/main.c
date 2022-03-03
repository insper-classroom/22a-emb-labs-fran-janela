/**
 * 5 semestre - Eng. da Computação - Insper
 * Rafael Corsi - rafael.corsi@insper.edu.br
 *
 * Projeto 0 para a placa SAME70-XPLD
 *
 * Objetivo :
 *  - Introduzir ASF e HAL
 *  - Configuracao de clock
 *  - Configuracao pino In/Out
 *
 * Material :
 *  - Kit: ATMEL SAME70-XPLD - ARM CORTEX M7
 */

/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include "asf.h"

/************************************************************************/
/* defines                                                              */
/************************************************************************/

#define LED_PIO              PIOC                    // periferico que controla o LED
#define LED_PIO_ID		     ID_PIOC			           // ID do periférico PIOC (controla LED)
#define LED_PIO_IDX          8                       // ID do LED no PIO
#define LED_PIO_IDX_MASK     (1 << LED_PIO_IDX)      // Mascara para CONTROLARMOS o LED

// Configuracoes do botao
#define BUT_PIO			     PIOA
#define BUT_PIO_ID           ID_PIOA
#define BUT_PIO_IDX		     11
#define BUT_PIO_IDX_MASK     (1u << BUT_PIO_IDX)     // esse já está pronto.

// Configs leds OLED1
#define LED1_PIO             PIOA             
#define LED1_PIO_ID		     ID_PIOA			  
#define LED1_PIO_IDX         0              
#define LED1_PIO_IDX_MASK    (1 << LED1_PIO_IDX)  

#define LED2_PIO             PIOC            
#define LED2_PIO_ID		     ID_PIOC	  
#define LED2_PIO_IDX         30              
#define LED2_PIO_IDX_MASK    (1 << LED2_PIO_IDX)  

#define LED3_PIO             PIOB             
#define LED3_PIO_ID		     ID_PIOB			  
#define LED3_PIO_IDX         2              
#define LED3_PIO_IDX_MASK    (1 << LED3_PIO_IDX)  

// Configs btns OLED1
#define BUT1_PIO			  PIOD
#define BUT1_PIO_ID           ID_PIOD
#define BUT1_PIO_IDX		  28
#define BUT1_PIO_IDX_MASK     (1u << BUT1_PIO_IDX)

#define BUT2_PIO			  PIOC
#define BUT2_PIO_ID           ID_PIOC
#define BUT2_PIO_IDX		  31
#define BUT2_PIO_IDX_MASK     (1u << BUT2_PIO_IDX)

#define BUT3_PIO			  PIOA
#define BUT3_PIO_ID           ID_PIOA
#define BUT3_PIO_IDX		  19
#define BUT3_PIO_IDX_MASK     (1u << BUT3_PIO_IDX)


/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void init(void);
void piscar(Pio *, const uint32_t);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

// Função de inicialização do uC
void init(void){
	// Initialize the board clock
	sysclk_init();

	// Desativa WatchDog Timer
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	// Ativa o PIO na qual o LED foi conectado
	// para que possamos controlar o LED.
	pmc_enable_periph_clk(LED_PIO_ID);
	pmc_enable_periph_clk(BUT_PIO_ID);

    //leds OLED1
    pmc_enable_periph_clk(LED1_PIO_ID);
    pmc_enable_periph_clk(LED2_PIO_ID);
    pmc_enable_periph_clk(LED3_PIO_ID);
    //btns OLED1
    pmc_enable_periph_clk(BUT1_PIO_ID);
    pmc_enable_periph_clk(BUT2_PIO_ID);
    pmc_enable_periph_clk(BUT3_PIO_ID);

	//Inicializa PC8 como saída
	pio_set_output(LED_PIO, LED_PIO_IDX_MASK, 0, 0, 0);

    //leds OLED1
    pio_set_output(LED1_PIO, LED1_PIO_IDX_MASK, 0, 0, 0);
    pio_set_output(LED2_PIO, LED2_PIO_IDX_MASK, 0, 0, 0);
    pio_set_output(LED3_PIO, LED3_PIO_IDX_MASK, 0, 0, 0);


    //Inicializa como entrada
	pio_set_input(BUT_PIO, BUT_PIO_IDX_MASK, PIO_DEFAULT);

    //btns OLED1
	pio_set_input(BUT1_PIO, BUT1_PIO_IDX_MASK, PIO_DEFAULT);
	pio_set_input(BUT2_PIO, BUT2_PIO_IDX_MASK, PIO_DEFAULT);
	pio_set_input(BUT3_PIO, BUT3_PIO_IDX_MASK, PIO_DEFAULT);

	

	pio_pull_up(BUT_PIO, BUT_PIO_IDX_MASK, 1);
	pio_pull_up(BUT1_PIO, BUT1_PIO_IDX_MASK, 1);
	pio_pull_up(BUT2_PIO, BUT2_PIO_IDX_MASK, 1);
	pio_pull_up(BUT3_PIO, BUT3_PIO_IDX_MASK, 1);
}

//função de piscar
void piscar(Pio *pio, const uint32_t mask){
    int cnt = 0;
    while(cnt<5){
        pio_set(pio, mask);      // Coloca 1 no pino LED
        delay_ms(200);                        // Delay por software de 200 ms
        pio_clear(pio, mask);    // Coloca 0 no pino do LED
        delay_ms(200);                        // Delay por software de 200 ms
        cnt++;
    }
}

/************************************************************************/
/* Main                                                                 */
/************************************************************************/

// Funcao principal chamada na inicalizacao do uC.
int main(void) {
	// inicializa sistema e IOs
	init();

	// super loop
	// aplicacoes embarcadas não devem sair do while(1).
	while (1)
	{
		
		_Bool btn = !pio_get(BUT_PIO, PIO_INPUT, BUT_PIO_IDX_MASK);
		_Bool btn1 = !pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK);
		_Bool btn2 = !pio_get(BUT2_PIO, PIO_INPUT, BUT2_PIO_IDX_MASK);
		_Bool btn3 = !pio_get(BUT3_PIO, PIO_INPUT, BUT3_PIO_IDX_MASK);


		if(btn)
			piscar(LED_PIO, LED_PIO_IDX_MASK);
        else if(btn1)
            piscar(LED1_PIO, LED1_PIO_IDX_MASK);
        else if(btn2)
            piscar(LED2_PIO, LED2_PIO_IDX_MASK);
        else if(btn3)
            piscar(LED3_PIO, LED3_PIO_IDX_MASK);
		else {
			pio_set(LED_PIO, LED_PIO_IDX_MASK);      // Coloca 1 no pino LED
			pio_set(LED1_PIO, LED1_PIO_IDX_MASK);      // Coloca 1 no pino LED
			pio_set(LED2_PIO, LED2_PIO_IDX_MASK);      // Coloca 1 no pino LED
			pio_set(LED3_PIO, LED3_PIO_IDX_MASK);      // Coloca 1 no pino LED
		}
	}
	return 0;
}

