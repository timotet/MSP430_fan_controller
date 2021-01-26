#include "msp430g2231.h"
#include "stdbool.h"
#include "stdint.h"

#define dled     BIT0               // p1.0 physical pin2 on chip
#define dledOff  P1OUT &= ~dled
#define dledOn   P1OUT |= dled
#define ADC      BIT3              // p1.3 physical pin5 on chip
//#define pwm      BIT0            // p1.0 physical pin2 on chip
//#define pwm      BIT6            // p1.6 physical pin8 on chip oops blown up!
#define pwm2     BIT6              // p2.6 physical pin 13 on chip
#define button   BIT5              // p1.5 physical pin 7 on chip
#define notUsedGPIO  BIT1 | BIT2 | BIT4  // Turn off all unused gpio's port 1

volatile uint16_t adc_average = 0;
uint16_t adc_read[10];
bool buttonPressed = true;

void init_gpio() {

	P1DIR |= dled | notUsedGPIO;         // set as output
	P2DIR |= BIT7;
	P1DIR &= ~button;      // input
	P1REN |= button;       // pull up
	P1REN &= ~notUsedGPIO; // pull down
	P2REN &= ~BIT7;
	P1OUT |= button;       // turn on pull up
	P1IES &= ~button;      // interrupt Edge Select - 0: trigger on rising edge, 1: trigger on falling edge
	P1IFG &= ~button;      // interrupt flag for p1.3 is off
	P1IE |= button;        // enable interrupt
	dledOff;                // turn led off
}

void init_timer() {

	// Configure Port Pins
//	P1DIR |= pwm;                        // p1.0 Output
//	P1SEL |= pwm;
	P2DIR |= pwm2;                       // p2.6 output
	P2SEL |= pwm2;
	P2DIR &= ~BIT7;                      // turn this off to get output on p2.6!!!!
	P2SEL &= ~BIT7;
	TACCR0 = 1024 - 1;                   // Period Register start timer 1024 for 10 bit ADC
    TACCR1 = 512;                        // 50% duty cycle to start
	TACTL = TASSEL_2 + MC_1 + ID_3;      // SMCLK, up mode divide by 8
//	TACCTL0 = OUTMOD_6;
	TACCTL1 = OUTMOD_6;                  // PWM output mode: 6 - PWM reset/set
}

void init_adc() {

	ADC10CTL0 |= ADC10SHT_1 + ADC10ON + SREF_0 + ADC10IE + MSC;   // VCC as reference, 8x clock
	ADC10CTL1 |= ADC10SSEL_0 + INCH_3 + CONSEQ_2;            // input channel A3 bit 3, repeat single channel
	ADC10AE0 |= ADC;                                        // P1.3 ADC option select
	ADC10DTC1 = 0xA;                                       // 30 conversions
}

void sample_adc() {

	ADC10CTL0 &= ~ENC;                      // stop conversion
	while (ADC10CTL1 & BUSY);               // Wait if ADC10 core is active
	ADC10SA = 0x200;                        // data transfer to the array
	ADC10CTL0 |= ENC + ADC10SC;             // Sampling and conversion start
	LPM0;
}

// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {

	LPM0_EXIT;

}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void) {

	LPM4_EXIT;
	dledOn;

	if (buttonPressed == false) {
		buttonPressed = true;
		TACTL = MC0;
		P2SEL &= ~pwm2;
		P2DIR |= pwm2;
		P2OUT &= ~pwm2;
		P1IES &= ~button;

	} else if (buttonPressed == true) {
		buttonPressed = false;
		P2DIR |= pwm2;
		P2SEL |= pwm2;
		TACTL = TASSEL_2 + MC_1 + ID_3;      // restart timer
		P1IES |= button;
	}
	P1IFG &= ~button;                        //clear interrupt flag
}

void main(void) {

	uint8_t i = 0;              // for averaging

	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	BCSCTL1 = CALBC1_1MHZ;      // calibrate basic clock system control 1 to 1mhz
	DCOCTL = CALDCO_1MHZ;       // calibrate DCO to 1mhz

	init_gpio();
	init_timer();
	init_adc();
	_enable_interrupts();

	while (1) {

		if (!buttonPressed) {

			sample_adc();
			for (i = 0; i < 10; i++) {
				adc_average += adc_read[i];
			}
			adc_average = (adc_average / 10);

			if (adc_average > 1023)
				adc_average = 1023;

			TACCR1 = adc_average;
			adc_average = 0;

		} else {

			dledOff;
			LPM4;
		}
	}
}
