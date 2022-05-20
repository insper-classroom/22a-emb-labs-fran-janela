/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"


/************************************************************************/
/*	FLAGS / STRUCTS / PROTOTYPES                                        */
/************************************************************************/

typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);

volatile char flag_edit_clk = 0;
volatile char flag_power = 0;

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg40);
LV_FONT_DECLARE(dseg25);

LV_IMG_DECLARE(clock1)
LV_IMG_DECLARE(steam)

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

static lv_obj_t * labelBtnPower;
static lv_obj_t * labelMenu;
static lv_obj_t * labelClk;
static lv_obj_t * labelHome;
static lv_obj_t * labelUp;
static lv_obj_t * labelDown;

lv_obj_t * labelFloor;
lv_obj_t * labelSetValue;
lv_obj_t * labelClock;
lv_obj_t * labelFloorDecimal;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

volatile SemaphoreHandle_t xSemaphoreRTC;

/************************************************************************/
/* LVGL HANDLER                                                         */
/************************************************************************/

static void power_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		flag_power = !flag_power;
	}
}

static void menu_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void steam_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void clk_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		flag_edit_clk = !flag_edit_clk;
	}
}

static void up_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	uint32_t current_hour, current_min, current_sec;
	if(code == LV_EVENT_CLICKED) {
		if(!flag_edit_clk){
			c = lv_label_get_text(labelSetValue);
			temp = atoi(c);
			lv_label_set_text_fmt(labelSetValue, "%02d", temp + 1);
		} else {
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			if (current_min == 59){
				current_min = 0;
				current_hour++;
			} else {
				current_min++;
			}
			rtc_set_time(RTC, current_hour, current_min, current_sec);
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(xSemaphoreRTC, &xHigherPriorityTaskWoken);
		}
	}
}

static void down_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	char *c;
	int temp;
	uint32_t current_hour, current_min, current_sec;
	if(code == LV_EVENT_CLICKED) {
		if(!flag_edit_clk){
			c = lv_label_get_text(labelSetValue);
			temp = atoi(c);
			lv_label_set_text_fmt(labelSetValue, "%02d", temp - 1);
		} else {
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			if (current_min == 0){
				current_min = 59;
				current_hour--;
			} else {
				current_min--;
			}
			rtc_set_time(RTC, current_hour, current_min, current_sec);
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(xSemaphoreRTC, &xHigherPriorityTaskWoken);
		}
	}
}

/************************************************************************/
/* RTC HANDLER                                                          */
/************************************************************************/

void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);

	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC)
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(xSemaphoreRTC, &xHigherPriorityTaskWoken);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
	}


	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}


/************************************************************************/
/* LVGL DESIGNS                                                         */
/************************************************************************/

void lv_termostato(void) {
	lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
	
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_NONE));
	lv_style_set_border_width(&style, 0);
	
	// POWER
	lv_obj_t * powerbtn = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(powerbtn, power_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(powerbtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	lv_obj_add_style(powerbtn, &style, 0);

	labelBtnPower = lv_label_create(powerbtn);
	lv_label_set_text(labelBtnPower, "[ " LV_SYMBOL_POWER);
	lv_obj_center(labelBtnPower);
	
	// MENU
	lv_obj_t * menubtn = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(menubtn, menu_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(menubtn, powerbtn, LV_ALIGN_OUT_RIGHT_MID, -5, -10);
	lv_obj_add_style(menubtn, &style, 0);

	labelMenu = lv_label_create(menubtn);
	lv_label_set_text(labelMenu, "| M |");
	lv_obj_center(labelMenu);
	
	// CLOCK
	lv_obj_t * clkbtn = lv_imgbtn_create(lv_scr_act());
	lv_obj_add_event_cb(clkbtn, clk_handler, LV_EVENT_ALL, NULL);
	lv_imgbtn_set_src(clkbtn, LV_IMGBTN_STATE_RELEASED, &clock1, NULL, NULL);
	lv_obj_align_to(clkbtn, menubtn, LV_ALIGN_OUT_RIGHT_MID, 2, 52);
	
	labelClk = lv_label_create(lv_scr_act());
	lv_obj_add_style(labelClk, &style, 0);
	lv_obj_align_to(labelClk, menubtn, LV_ALIGN_OUT_RIGHT_MID, 35, 0);
	lv_label_set_text_fmt(labelClk, "]");
	
	
	// UP
	lv_obj_t * btnUp = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnUp, up_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(btnUp, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
	lv_obj_add_style(btnUp, &style, 0);

	labelUp = lv_label_create(btnUp);
	lv_label_set_text(labelUp, LV_SYMBOL_UP " ]");
	lv_obj_center(labelUp);
	
	// DOWN
	lv_obj_t * btnDown = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btnDown, down_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btnDown, btnUp, LV_ALIGN_OUT_LEFT_MID, -30, -10);
	lv_obj_add_style(btnDown, &style, 0);

	labelDown = lv_label_create(btnDown);
	lv_label_set_text(labelDown, "[ " LV_SYMBOL_DOWN " |");
	lv_obj_center(labelDown);
	
	// HOME
	lv_obj_t * labelHome = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(labelHome, clk_handler, LV_EVENT_ALL, NULL);
	lv_obj_align_to(labelHome, btnDown, LV_ALIGN_OUT_TOP_LEFT, -40, -25);
	lv_obj_add_style(labelHome, &style, 0);

	labelHome = lv_label_create(labelHome);
	lv_label_set_text(labelHome, LV_SYMBOL_HOME);
	lv_obj_center(labelHome);
	
	// CURRENT TEMP
	labelFloor = lv_label_create(lv_scr_act());
	lv_obj_align(labelFloor, LV_ALIGN_LEFT_MID, 35 , -20);
	lv_obj_set_style_text_font(labelFloor, &dseg70, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelFloor, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelFloor, "%02d", 23);
	
	// CURRENT TEMP DECIMAL
	labelFloorDecimal = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelFloorDecimal, labelFloor, LV_ALIGN_OUT_RIGHT_MID, 3 , 24);
	lv_obj_set_style_text_font(labelFloorDecimal, &dseg25, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelFloorDecimal, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelFloorDecimal, ". %01d", 4);
	
	// SET TEMP
	labelSetValue = lv_label_create(lv_scr_act());
	lv_obj_align(labelSetValue, LV_ALIGN_RIGHT_MID, -20 , -40);
	lv_obj_set_style_text_font(labelSetValue, &dseg40, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelSetValue, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelSetValue, "%02d", 23);
	
	// CLOCK
	labelClock = lv_label_create(lv_scr_act());
	lv_obj_align(labelClock, LV_ALIGN_TOP_RIGHT, -10 , 10);
	lv_obj_set_style_text_font(labelClock, &dseg25, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelClock, "%02d:%02d", 10, 40);
	
	// STEAM
	lv_obj_t * steambtn = lv_imgbtn_create(lv_scr_act());
	lv_obj_add_event_cb(steambtn, steam_handler, LV_EVENT_ALL, NULL);
	lv_imgbtn_set_src(steambtn, LV_IMGBTN_STATE_RELEASED, &steam, NULL, NULL);
	lv_obj_align(steambtn, LV_ALIGN_RIGHT_MID, 85, 55);
	
	// CLOCK 2
	lv_obj_t * clock2btn = lv_imgbtn_create(lv_scr_act());
	lv_obj_add_event_cb(clock2btn, clk_handler, LV_EVENT_ALL, NULL);
	lv_imgbtn_set_src(clock2btn, LV_IMGBTN_STATE_RELEASED, &clock1, NULL, NULL);
	lv_obj_align(clock2btn, LV_ALIGN_RIGHT_MID, 50, 55);
}

void lv_termostato_off(void)
{
	lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
	
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_NONE));
	lv_style_set_border_width(&style, 0);

	// POWER
	lv_obj_t * powerbtn = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(powerbtn, power_handler, LV_EVENT_ALL, NULL);
	lv_obj_align(powerbtn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	lv_obj_add_style(powerbtn, &style, 0);

	labelBtnPower = lv_label_create(powerbtn);
	lv_label_set_text(labelBtnPower, "[ " LV_SYMBOL_POWER " ]");
	lv_obj_center(labelBtnPower);	
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;
	char on = 1;

	lv_termostato();

	for (;;)  {
		if(on && flag_power){
			lv_obj_clean(lv_scr_act());
			lv_termostato_off();
			on = !on;
		} else if (!on && !flag_power){
			lv_obj_clean(lv_scr_act());
			lv_termostato();
			on = !on;
		}
		lv_tick_inc(50);
		lv_task_handler();
		vTaskDelay(50);
	}
}

void task_clock(void *pvParameters) {
	/* RTC init */
	calendar rtc_initial = {2022, 5, 20, 12, 15, 45 ,1};
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_SR_SEC|RTC_SR_ALARM);
	uint32_t current_hour, current_min, current_sec;
	char points = 1;

	while (1)
	{
		if (xSemaphoreTake(xSemaphoreRTC, 1000 / portTICK_PERIOD_MS)){
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			if(points){
				lv_label_set_text_fmt(labelClock, "%02d:%02d", current_hour, current_min);
			} else {
				lv_label_set_text_fmt(labelClock, "%02d %02d", current_hour, current_min);
			}
			points = !points;
		}
	}
	
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/************************************************************************/
/* inits                                                                */
/************************************************************************/

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

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	data->point.x = px;
	data->point.y = py;
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	configure_touch();
	configure_lvgl();
	
	xSemaphoreRTC = xSemaphoreCreateBinary();


	/* Create task to control oled */
	if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create lcd task\r\n");
	}
	
	/* Create task to update clock */
	if (xTaskCreate(task_clock, "CLOCK", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create clock task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
