#include "asf.h"
#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/************************************************************************/
/* DEFINES                                                              */
/************************************************************************/
/**
 *  Informacoes para o RTC
 *  poderia ser extraida do __DATE__ e __TIME__
 *  ou ser atualizado pelo PC.
 */
typedef struct  {
  uint32_t year;
  uint32_t month;
  uint32_t day;
  uint32_t week;
  uint32_t hour;
  uint32_t minute;
  uint32_t second;
} calendar;

// Config LED
#define LED_PIO					      PIOC                  
#define LED_PIO_ID				    ID_PIOC			     
#define LED_PIO_IDX				    8                       
#define LED_PIO_IDX_MASK		  (1 << LED_PIO_IDX)

// Config LEDs OLED1
#define LED1_PIO				      PIOA
#define LED1_PIO_ID				    ID_PIOA
#define LED1_PIO_IDX			    0
#define LED1_PIO_IDX_MASK		  (1u << LED1_PIO_IDX)

#define LED2_PIO				      PIOC
#define LED2_PIO_ID				    ID_PIOC
#define LED2_PIO_IDX			    30
#define LED2_PIO_IDX_MASK		  (1u << LED2_PIO_IDX)

#define LED3_PIO				      PIOB
#define LED3_PIO_ID				    ID_PIOB
#define LED3_PIO_IDX			    2
#define LED3_PIO_IDX_MASK		  (1u << LED3_PIO_IDX)

// Config buttons OLED1
#define BUTTON_1_PIO			    PIOD
#define BUTTON_1_PIO_ID       ID_PIOD
#define BUTTON_1_PIO_IDX		  28
#define BUTTON_1_PIO_IDX_MASK (1u << BUTTON_1_PIO_IDX)

/************************************************************************/
/* VAR globais                                                          */
/************************************************************************/

volatile char flag_rtt_alarm = 0;
volatile char flag_rtc_alarm = 0;
volatile char flag_button = 0;
volatile char flag_rtc_sec = 0;


/************************************************************************/
/* PROTOTYPES                                                           */
/************************************************************************/

void init(void);
void pin_toggle(Pio *pio, uint32_t mask);
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);
void display_date(uint32_t, uint32_t, uint32_t);

/************************************************************************/
/* Handlers                                                             */
/************************************************************************/

void button_callback(){
  flag_button = 1;
}

void TC0_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 0);
	pin_toggle(LED3_PIO, LED3_PIO_IDX_MASK);
}

void TC1_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 1);
	pin_toggle(LED1_PIO, LED1_PIO_IDX_MASK);
}

void TC2_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 2);
	pin_toggle(LED_PIO, LED_PIO_IDX_MASK);
}



void RTT_Handler(void) {
	uint32_t ul_status;

	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		RTT_init(4, 0, RTT_MR_RTTINCIEN);
		pin_toggle(LED2_PIO, LED2_PIO_IDX_MASK);
	}
	
	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
	}
}


void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		flag_rtc_sec = 1;
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		flag_rtc_alarm = 1;
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

/************************************************************************/
/* Funcoes                                                              */
/************************************************************************/

void init(void) {
	// Initialize the board clock
	sysclk_init();

	// Deactivate WatchDog Timer
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	// LED
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_set_output(LED_PIO, LED_PIO_IDX_MASK, 1, 0, 1);
	
	// LEDs OLED1
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_set_output(LED1_PIO, LED1_PIO_IDX_MASK, 1, 0, 1);
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_set_output(LED2_PIO, LED2_PIO_IDX_MASK, 1, 0, 1);
	pmc_enable_periph_clk(LED3_PIO_ID);
	pio_set_output(LED3_PIO, LED3_PIO_IDX_MASK, 1, 0, 1);
	
	
	// Button:
	pmc_enable_periph_clk(BUTTON_1_PIO_ID);
	pio_configure(BUTTON_1_PIO, PIO_INPUT, BUTTON_1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUTTON_1_PIO, BUTTON_1_PIO_IDX_MASK, 60);
	pio_handler_set(BUTTON_1_PIO,
	BUTTON_1_PIO_ID,
	BUTTON_1_PIO_IDX_MASK,
	PIO_IT_FALL_EDGE,
	button_callback);
	pio_enable_interrupt(BUTTON_1_PIO, BUTTON_1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUTTON_1_PIO);
	NVIC_EnableIRQ(BUTTON_1_PIO_ID);
	NVIC_SetPriority(BUTTON_1_PIO_ID, 4);
}

/**
* @Brief Inverte o valor do pino 0->1/ 1->0
*/
void pin_toggle(Pio *pio, uint32_t mask) {
  if(pio_get_output_data_status(pio, mask))
    pio_clear(pio, mask);
  else
    pio_set(pio,mask);
}

/**
* Configura TimerCounter (TC) para gerar uma interrupcao no canal (ID_TC e TC_CHANNEL)
* na taxa de especificada em freq.
* O TimerCounter é meio confuso
* o uC possui 3 TCs, cada TC possui 3 canais
*	TC0 : ID_TC0, ID_TC1, ID_TC2
*	TC1 : ID_TC3, ID_TC4, ID_TC5
*	TC2 : ID_TC6, ID_TC7, ID_TC8
*
**/
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	/* Configura o PMC */
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura NVIC*/
  	NVIC_SetPriority(ID_TC, 4);
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
}

/** 
 * Configura RTT
 *
 * arg0 pllPreScale  : Frequência na qual o contador irá incrementar
 * arg1 IrqNPulses   : Valor do alarme 
 * arg2 rttIRQSource : Pode ser uma 
 *     - 0: 
 *     - RTT_MR_RTTINCIEN: Interrupção por incremento (pllPreScale)
 *     - RTT_MR_ALMIEN : Interrupção por alarme
 */
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

/**
* Configura o RTC para funcionar com interrupcao de alarme
*/
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}

void display_date(uint32_t hour, uint32_t min, uint32_t sec){
	char date_str[128];
	sprintf(date_str, "%02d:%02d:%02d", hour, min, sec);
	gfx_mono_draw_string(date_str, 5, 5, &sysfont);
}

/************************************************************************/
/* Main Code	                                                        */
/************************************************************************/
int main(void){
  board_init();
	delay_init();

  init();

	gfx_mono_ssd1306_init();

	TC_init(TC0, ID_TC1, 1, 5);
	tc_start(TC0, 1);

	TC_init(TC0, ID_TC2, 2, 4);
	tc_start(TC0, 2);

	RTT_init(4, 16, RTT_MR_ALMIEN); 
	
	calendar rtc_initial = {2018, 3, 19, 12, 15, 45 ,1};
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_SR_SEC|RTC_SR_ALARM);


	uint32_t current_hour, current_min, current_sec;
	uint32_t current_year, current_month, current_day, current_week;

	while (1) {
		if (flag_rtc_sec) {
			flag_rtc_sec = 0;
			rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			display_date(current_hour, current_min, current_sec);
		}
		if (flag_button) {
			uint32_t next_min, next_sec;

			next_sec = current_sec + 20;

			if (next_sec >= 60){
				next_min = current_min + 1;
				next_sec = next_sec - 60;
			}
			else{
				next_min = current_min;
			}

			rtc_set_time_alarm(RTC, 1, current_hour, 1, next_min, 1, next_sec);

			flag_button = 0;
		}
		if (flag_rtc_alarm) {
			flag_rtc_alarm = 0;
			TC_init(TC0, ID_TC0, 0, 4);
			tc_start(TC0, 0);
		}
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}