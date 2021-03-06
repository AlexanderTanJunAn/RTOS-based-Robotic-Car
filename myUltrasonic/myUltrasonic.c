#include "MKL25Z4.h"                    // Device header
#include <math.h>

#define PTD2_Pin 2  // output Trigger
#define PTD3_Pin 3 // input Echo 
#define PTD4_Pin 4 // PWM pin

#define MASK(x) (1 << (x))

volatile int ready = 0;
volatile uint16_t start = 0;
volatile uint16_t startRecord = 0;
volatile uint16_t endRecord = 0;
volatile uint16_t valueRecord = 0;


volatile uint16_t end = 0;
volatile uint16_t counter = 0;
volatile uint16_t read = 0;
volatile uint16_t num = 0;

volatile uint16_t maincounter = 0;

void initUltrasonic() {
	
	//Enable clock gating for portD
	SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
	
	//Set PTD2 as GPIO output to trigger the sensor
	PORTD->PCR[PTD2_Pin] &= ~PORT_PCR_MUX_MASK;
	PORTD->PCR[PTD2_Pin] |= PORT_PCR_MUX(1);
	PTD->PDDR |= MASK(PTD2_Pin);

	//Set PTD3 pin to TPM0_CH3 mode and pin to input
	PORTD->PCR[PTD3_Pin] &= ~PORT_PCR_MUX_MASK;
	PORTD->PCR[PTD3_Pin] |= (PORT_PCR_MUX(4));
	PTD->PDDR &= ~(MASK(PTD3_Pin)); //??
	
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	//Select Clock for TPM module
	SIM->SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK;
	SIM->SOPT2 |= SIM_SOPT2_TPMSRC(1);
	
  // 375000 / 7500 = 50 Hz [basically when reach 7500 (max count) will flip over to 0 and start again]
	TPM0->MOD = 7500;
	

	//Edge-Aligned PWM
  //CMOD - 1 and PS - 111 (128)
  TPM0_SC &= ~((TPM_SC_CMOD_MASK) | (TPM_SC_PS_MASK));
  TPM0_SC |= (TPM_SC_CMOD(1) | TPM_SC_PS(7)); //CMOD = 1 => LPTPM counter increments on every LPTPM counter clock
  TPM0_SC &= ~(TPM_SC_CPWMS_MASK); //count up by default (0)
	
	//Configure channel 3 from TPM0 as input capture for both edges and enabling interrupt
	TPM0_C3SC &= ~((TPM_CnSC_ELSB_MASK) | (TPM_CnSC_ELSA_MASK) | (TPM_CnSC_MSB_MASK) | (TPM_CnSC_MSA_MASK) | (TPM_CnSC_CHIE_MASK));
	TPM0_C3SC |= (TPM_CnSC_ELSB(1) | TPM_CnSC_ELSA(1) | TPM_CnSC_CHIE(1));
	
	//Enable TPM0 interrupt
	NVIC_SetPriority(TPM0_IRQn, 2);
	NVIC_ClearPendingIRQ(TPM0_IRQn); 
	NVIC_EnableIRQ(TPM0_IRQn); 

	//Disable Clk to avoid interrupt triggers
	SIM->SCGC6 &= ~SIM_SCGC6_TPM0_MASK;

}

static void delay(volatile uint32_t nof) {
  while(nof!=0) {
    __asm("NOP");
    nof--;
  }
}

void ultrasonicTrigger(){
	
	//starting trigger
	PTD->PDOR |= MASK(PTD2_Pin);
	delay(0x11); //10us orginal = 0x11
	
	//Off trigger after 10 seconds
	PTD->PDOR &= ~MASK(PTD2_Pin);
	delay(0x18e70); // 60ms original = 0x18e70
}

int readUltrasonic(void){
	float value = 0;
	
	//Enable Clk Source to TPM0 module
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	
	ultrasonicTrigger();
	
	while(!ready){}; //measuring when echo comesback
	value = end - startRecord;
	start = 0;
	value = (value * 2.6666 * 0.01715 * 1.5) - 337 ;  // determine speed of ultrasonic from frequency and speedy of sound = 34300cm/s /2 =17150;
	
	//Disable Clk to avoid interrupt triggers
	SIM->SCGC6 &= ~SIM_SCGC6_TPM0_MASK;
	counter = 0;
		
	return value;
}

void TPM0_IRQHandler (void) // what is the interrupt logic
{
	NVIC_ClearPendingIRQ(TPM0_IRQn); //clear queue before begin;
		
	if(TPM0_C3SC & TPM_CnSC_CHF_MASK){  // If CHF = channel is 1 when an event occur- cleared by writing 1 
		TPM0_C3SC |= TPM_CnSC_CHF_MASK;
		if(!start){
			/*Clear the TPM0 counter register to start with ~0 */
			TPM0_CNT = 0x00000000000;
			startRecord = TPM0_CNT; 
			start = 1;
		} else {
			end = TPM0_C3V + counter * 7500; //tpm0_c3V is counter value
			ready = 1;
		}
	}
	if(TPM0_SC & TPM_SC_TOF_MASK) { //checking for overflow so that it resets when hit and counter = 1 cycle.
		TPM0_SC |= TPM_SC_TOF_MASK;
		counter++; 
	}
}

int main(void)
{
	SystemCoreClockUpdate();
	
	initUltrasonic(); 

	while(1)
	{
		maincounter++; 
		read = readUltrasonic(); 
		delay(0x18e70); // 1ms? 1ms / (128/48Mhz)
	}
}
