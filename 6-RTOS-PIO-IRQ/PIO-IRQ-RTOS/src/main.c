#include "conf_board.h"
#include <asf.h>

/************************************************************************/
/* BOARD CONFIG                                                         */
/************************************************************************/

#define BUT_PIO						  PIOA
#define BUT_PIO_ID					  ID_PIOA
#define BUT_PIO_IDX					  11
#define BUT_PIO_IDX_MASK			  (1 << BUT_PIO_IDX)

#define LED_PIO						  PIOC
#define LED_PIO_ID					  ID_PIOC
#define LED_PIO_IDX					  8
#define LED_PIO_IDX_MASK              (1 << LED_PIO_IDX)

// Config LEDs OLED1
#define LED1_PIO				      PIOA
#define LED1_PIO_ID				      ID_PIOA
#define LED1_PIO_IDX			      0
#define LED1_PIO_IDX_MASK		      (1u << LED1_PIO_IDX)

#define LED2_PIO				      PIOC
#define LED2_PIO_ID				      ID_PIOC
#define LED2_PIO_IDX			      30
#define LED2_PIO_IDX_MASK		      (1u << LED2_PIO_IDX)

#define LED3_PIO				      PIOB
#define LED3_PIO_ID				      ID_PIOB
#define LED3_PIO_IDX			      2
#define LED3_PIO_IDX_MASK		      (1u << LED3_PIO_IDX)

// Config buttons OLED1
#define BUT1_PIO					  PIOD
#define BUT1_PIO_ID					  ID_PIOD
#define BUT1_PIO_IDX				  28
#define BUT1_PIO_IDX_MASK			  (1u << BUT1_PIO_IDX)

#define BUT2_PIO					  PIOC
#define BUT2_PIO_ID                   ID_PIOC
#define BUT2_PIO_IDX				  31
#define BUT2_PIO_IDX_MASK			  (1u << BUT2_PIO_IDX)

#define BUT3_PIO					  PIOA
#define BUT3_PIO_ID					  ID_PIOA
#define BUT3_PIO_IDX				  19
#define BUT3_PIO_IDX_MASK			  (1u << BUT3_PIO_IDX)


#define USART_COM_ID ID_USART1
#define USART_COM USART1

/************************************************************************/
/* RTOS                                                                */
/************************************************************************/

#define TASK_LED_STACK_SIZE (1024 / sizeof(portSTACK_TYPE))
#define TASK_LED_STACK_PRIORITY (tskIDLE_PRIORITY)
#define TASK_BUT_STACK_SIZE (2048 / sizeof(portSTACK_TYPE))
#define TASK_BUT_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                          signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************************************************************/
/* recursos RTOS                                                        */
/************************************************************************/

/** Semaforo a ser usado pela task led */
SemaphoreHandle_t xSemaphoreBut;
SemaphoreHandle_t xSemaphoreBut1;
SemaphoreHandle_t xSemaphoreBut2;
SemaphoreHandle_t xSemaphoreBut3;

/** Queue for msg log send data */
QueueHandle_t xQueueLedFreq;

/************************************************************************/
/* prototypes local                                                     */
/************************************************************************/

void but_callback(void);
void but1_callback(void);
void but2_callback(void);
void but3_callback(void);

static void BUT_init(void);
void pin_toggle(Pio *pio, uint32_t mask);
static void USART1_init(void);
void LED_init(int estado);

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

/**
 * \brief Called if stack overflow during execution
 */
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
                                          signed char *pcTaskName) {
  printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
  /* If the parameters have been corrupted then inspect pxCurrentTCB to
   * identify which task has overflowed its stack.
   */
  for (;;) {
  }
}

/**
 * \brief This function is called by FreeRTOS idle task
 */
extern void vApplicationIdleHook(void) { pmc_sleep(SAM_PM_SMODE_SLEEP_WFI); }

/**
 * \brief This function is called by FreeRTOS each tick
 */
extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void) {
  /* Called if a call to pvPortMalloc() fails because there is insufficient
  free memory available in the FreeRTOS heap.  pvPortMalloc() is called
  internally by FreeRTOS API functions that create tasks, queues, software
  timers, and semaphores.  The size of the FreeRTOS heap is set by the
  configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

  /* Force an assert. */
  configASSERT((volatile void *)NULL);
}

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

void but_callback(void) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(xSemaphoreBut, &xHigherPriorityTaskWoken);
}

void but1_callback(void) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(xSemaphoreBut1, &xHigherPriorityTaskWoken);
}

void but2_callback(void) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(xSemaphoreBut2, &xHigherPriorityTaskWoken);
}

void but3_callback(void) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(xSemaphoreBut3, &xHigherPriorityTaskWoken);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_led(void *pvParameters) {

  LED_init(1);

  uint32_t msg = 0;
  uint32_t delayMs = 2000;

  /* tarefas de um RTOS n�o devem retornar */
  for (;;) {
    /* verifica se chegou algum dado na queue, e espera por 0 ticks */
    if (xQueueReceive(xQueueLedFreq, &msg, (TickType_t)0)) {
      /* chegou novo valor, atualiza delay ! */
      /* aqui eu poderia verificar se msg faz sentido (se esta no range certo)
       */
      /* converte ms -> ticks */
      delayMs = msg / portTICK_PERIOD_MS;
      printf("delay = %d \n", delayMs);
    }

    /* pisca LED */
    pin_toggle(LED_PIO, LED_PIO_IDX_MASK);

    /* suspende por delayMs */
    vTaskDelay(delayMs);
  }
}

static void task_but(void *pvParameters) {

  /* iniciliza botao */
  BUT_init();

  uint32_t delayTicks = 2000;

  for (;;) {
    /* aguarda por tempo inderteminado at� a liberacao do semaforo */
    if (xSemaphoreTake(xSemaphoreBut, 1000)) {
	    /* atualiza frequencia */
	    delayTicks -= 100;

	    /* envia nova frequencia para a task_led */
	    xQueueSend(xQueueLedFreq, (void *)&delayTicks, 10);

	    /* garante range da freq. */
	    if (delayTicks == 100) {
		    delayTicks = 900;
	    }
    } else if (xSemaphoreTake(xSemaphoreBut1, 1000)) {
		 /* atualiza frequencia */
		 delayTicks += 100;

		 /* envia nova frequencia para a task_led */
		 xQueueSend(xQueueLedFreq, (void *)&delayTicks, 10);

		 /* garante range da freq. */
		 if (delayTicks == 3000) {
			 delayTicks = 2000;
		 }
	}
  }
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

/**
 * \brief Configure the console UART.
 */
static void configure_console(void) {
  const usart_serial_options_t uart_serial_options = {
      .baudrate = CONF_UART_BAUDRATE,
      .charlength = CONF_UART_CHAR_LENGTH,
      .paritytype = CONF_UART_PARITY,
      .stopbits = CONF_UART_STOP_BITS,
  };

  /* Configure console UART. */
  stdio_serial_init(CONF_UART, &uart_serial_options);

  /* Specify that stdout should not be buffered. */
  setbuf(stdout, NULL);
}

void pin_toggle(Pio *pio, uint32_t mask) {
  if (pio_get_output_data_status(pio, mask))
    pio_clear(pio, mask);
  else
    pio_set(pio, mask);
}


/************************************************************************/
/* INIT                                                                 */
/************************************************************************/

void configure_output(Pio *p_pio, const pio_type_t ul_type, const uint32_t ul_mask, const uint32_t ul_attribute, uint32_t ul_id ){
	pmc_enable_periph_clk(ul_id);
	pio_configure(p_pio, ul_type, ul_mask, ul_attribute);
}

void configure_input(Pio *p_pio, const pio_type_t ul_type, const uint32_t ul_mask, const uint32_t ul_attribute, uint32_t ul_id){
	pmc_enable_periph_clk(ul_id);
	pio_configure(p_pio, ul_type, ul_mask, ul_attribute);
	pio_set_debounce_filter(p_pio, ul_mask, 60);
}

void configure_interruption(Pio *p_pio, uint32_t ul_id, uint32_t ul_mask, uint32_t ul_attr, void (*p_handler) (uint32_t, uint32_t)){
	pio_handler_set(p_pio, ul_id, ul_mask, ul_attr, p_handler);
	pio_enable_interrupt(p_pio, ul_mask);
	pio_get_interrupt_status(p_pio);
	NVIC_EnableIRQ(ul_id);
	NVIC_SetPriority(ul_id, 5);
}

void LED_init(int estado){
	// LEDs OLED1
	configure_output(LED_PIO, PIO_OUTPUT_0, LED_PIO_IDX_MASK, PIO_DEFAULT, LED_PIO_ID);
	configure_output(LED1_PIO, PIO_OUTPUT_0, LED1_PIO_IDX_MASK, PIO_DEFAULT, LED1_PIO_ID);
	configure_output(LED2_PIO, PIO_OUTPUT_0, LED2_PIO_IDX_MASK, PIO_DEFAULT, LED2_PIO_ID);
	configure_output(LED3_PIO, PIO_OUTPUT_0, LED3_PIO_IDX_MASK, PIO_DEFAULT, LED2_PIO_ID);
}

void BUT_init(void){
	configure_input(BUT_PIO, PIO_INPUT, BUT_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE, BUT_PIO_ID);
	configure_input(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE, BUT1_PIO_ID);
	configure_input(BUT2_PIO, PIO_INPUT, BUT2_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE, BUT2_PIO_ID);
	configure_input(BUT3_PIO, PIO_INPUT, BUT3_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE, BUT3_PIO_ID);
	
	configure_interruption(BUT_PIO, BUT_PIO_ID, BUT_PIO_IDX_MASK, PIO_IT_FALL_EDGE, but_callback);
	configure_interruption(BUT1_PIO, BUT1_PIO_ID, BUT1_PIO_IDX_MASK, PIO_IT_FALL_EDGE, but1_callback);
	configure_interruption(BUT2_PIO, BUT2_PIO_ID, BUT2_PIO_IDX_MASK, PIO_IT_FALL_EDGE, but2_callback);
	configure_interruption(BUT3_PIO, BUT3_PIO_ID, BUT3_PIO_IDX_MASK, PIO_IT_FALL_EDGE, but3_callback);
}


/************************************************************************/
/* main                                                                 */
/************************************************************************/

/**
 *  \brief FreeRTOS Real Time Kernel example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void) {
	// Initialize the board clock
	sysclk_init();
	board_init();
	configure_console();

	/* Attempt to create a semaphore. */
	xSemaphoreBut = xSemaphoreCreateBinary();
	if (xSemaphoreBut == NULL)
		printf("falha em criar o semaforo \n");

	/* Attempt to create a semaphore. */
	xSemaphoreBut1 = xSemaphoreCreateBinary();
	if (xSemaphoreBut1 == NULL)
		printf("falha em criar o semaforo \n");

	/* Attempt to create a semaphore. 	
	xSemaphoreBut2 = xSemaphoreCreateBinary();
	if (xSemaphoreBut2 == NULL)
		printf("falha em criar o semaforo \n");
		
	Attempt to create a semaphore. 	
	xSemaphoreBut3 = xSemaphoreCreateBinary();
	if (xSemaphoreBut3 == NULL)
		printf("falha em criar o semaforo \n");*/

	/* cria queue com 32 "espacos" */
	/* cada espa�o possui o tamanho de um inteiro*/
	xQueueLedFreq = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueLedFreq == NULL)
		printf("falha em criar a queue \n");

	/* Create task to make led blink */
	if (xTaskCreate(task_led, "Led", TASK_LED_STACK_SIZE, NULL,
                  TASK_LED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create test led task\r\n");
	}

	/* Create task to monitor processor activity */
	if (xTaskCreate(task_but, "BUT", TASK_BUT_STACK_SIZE, NULL,
                  TASK_BUT_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}	

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* RTOS n�o deve chegar aqui !! */
	while (1) {
	}

	/* Will only get here if there was insufficient memory to create the idle
	* task. */
	return 0;
}
