#include "lcd/lcd.h"
#include "gd32v_pjt_include.h"

extern uint32_t enable_mcycle_minstret();

/*
   ----------------------------------------------------------------------
   From: GD32VF103_Firmware_Library/Examples/TIMER/TIMER1_timebase/main.c
   ----------------------------------------------------------------------
*/
void timer_config(void)
{
	/* ----------------------------------------------------------------------------
	   TIMER1 Configuration: 
	   TIMER1CLK = SystemCoreClock/5400 = 20KHz.
	   TIMER1 configuration is timing mode, and the timing is 0.2s(4000/20000 = 0.2s).
	   CH0 update rate = TIMER1 counter clock/CH0CV = 20000/4000 = 5Hz.
	   ---------------------------------------------------------------------------- */
	timer_oc_parameter_struct	timer_ocinitpara;
	timer_parameter_struct		timer_initpara;

	rcu_periph_clock_enable(RCU_TIMER1);

	timer_deinit(TIMER1);
	/* initialize TIMER init parameter struct */
	timer_struct_para_init(&timer_initpara);
	
	/* TIMER1 configuration */
	timer_initpara.prescaler         = 0x5399;
	timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
	timer_initpara.counterdirection  = TIMER_COUNTER_UP;
	timer_initpara.period            = 0x4000;
	timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
	timer_init(TIMER1, &timer_initpara);

	/* initialize TIMER channel output parameter struct */
	timer_channel_output_struct_para_init(&timer_ocinitpara);

	/* CH0,CH1 and CH2 configuration in OC timing mode */
	timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
	timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
	timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
	timer_channel_output_config(TIMER1, TIMER_CH_0, &timer_ocinitpara);

	/* CH0 configuration in OC timing mode */
	timer_channel_output_pulse_value_config(TIMER1, TIMER_CH_0, 2000);
	timer_channel_output_mode_config(TIMER1, TIMER_CH_0, TIMER_OC_MODE_TIMING);
	timer_channel_output_shadow_config(TIMER1, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

	timer_interrupt_enable(TIMER1, TIMER_INT_CH0);
	timer_enable(TIMER1);
}

char hexval(unsigned char val)
{
	if(val < 10)
		return '0' + val;
	return 'A' + (val - 10);
}

/*
 * Print the 32-bit hex value to LCD at the specified line number.
 * The value is shown in big-endian form.
 */
void printHex(unsigned char line, unsigned long val) {

	char buf[10];

	for(int i=0; i < 4; i++) {
		buf[i]   = hexval((val >> (12 - (i << 2))) & 0xf);
		buf[i+5] = hexval((val >> (28 - (i << 2))) & 0xf);
	}
	buf[4] = ':';
	buf[9] = 0;

	LCD_ShowString(24, (line << 4), (u8 *)(buf), YELLOW);
}

unsigned long g_count = 0; // Displays starting at 2 - why?

#define read_csr(csr_reg) ({ \
	unsigned long __tmp; \
	asm volatile ("csrr %0, " #csr_reg : "=r"(__tmp)); \
	__tmp; \
})

int main(void)
{
	// RGB LED: LED_R->PORTC_13, LED_G->PORTA_1, LED_B->PORTA_2
	// Enable PORTA and PORTC peripherals.
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOC);

	// Configure bits 1 & 2 of PORTA, and bit 13 in PORTC
	// Note, the SDK defining GPIO 'PIN' is a misnomer.
	gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
	gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

	timer_config();

	eclic_global_interrupt_enable();
	eclic_set_nlbits(ECLIC_GROUP_LEVEL3_PRIO1);
	eclic_irq_enable(TIMER1_IRQn,1,0);

	Lcd_Init();			// init OLED
	BACK_COLOR=BLUE;
	LCD_Clear(BACK_COLOR);

	LEDR(1);
	LEDG(1);
	LEDB(1);

	g_count = 0;
	unsigned long *rom_ptr = (unsigned long *)(0x1fffb000);
	int i=0;

	enable_mcycle_minstret();

	while (1)
	{
		// mcycle (read only, User mode)
		// mcyclel = read_csr(0x301);
		unsigned long mcyclel = read_csr(0xC00);

		// Flash Memory Controller: Memory density register
		unsigned long fmc_pid = *(unsigned long *)(0x1FFFF7E0);

		printHex(0, mcyclel);
		printHex(1, *rom_ptr);
		printHex(2, g_count);
		printHex(3, *(unsigned short *)(0x40000024));
		printHex(4, fmc_pid);

		if(++rom_ptr == (unsigned long *)0x1fffb800)
			rom_ptr = (unsigned long *)0x1fffb000;

		//printHex(1, *(unsigned long *)(0x40001824));
		//printHex(0, *(unsigned long *)(0x40001c24));

		delay_1ms(100);
		
		i++;

		if(i % 2 == 0)
			continue;

		switch( (i / 2) % 3 ) {
			case 0:
				LEDR_TOG;
				break;
			case 1:
				LEDG_TOG;
				break;
			case 2:
				LEDB_TOG;
				break;
		}
	}
}

