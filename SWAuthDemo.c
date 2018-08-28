/*
 * @brief Blinky example using sysTick
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "board.h"
#include "lib_crc16.h"
#include "string.h"
#include "Air202.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#define AUTH_ENABLE                (1)

#define GPRS_CTL_PORT       (3)
#define GPRS_CTL_PIN        (3)

#define LED_USED            LED_GREEN
#define AT_UART             LPC_UART2
#define AT_UART_IRQHandler  UART2_IRQHandler

#define TICKRATE_HZ         (1000)	    /* 1000 ticks per second */
#define UID_ADDR            (0xF000)
#define UID_SIZE            (32)
#define AUTH_PERIOD         (10*1000)   /* 10000 miniseconds */
#define AT_UART_BAUDRATE    (115200)
#define TX_RB_SIZE          (256)
#define RX_RB_SIZE          (256)
#define SQ_DEADLINE         (10)

#define SERVER_IP           "orange.55555.io"
#define SERVER_PORT         31318
//#define SERVER_PORT         28581


enum GPRS_ERROR_CODE{
	GPRS_SUCCESS = 0,
	GPRS_POWER_ON_FAIL = -1,
	GPRS_SIM_NOT_READY = -2,
	GPRS_SIGNAL_POOR = -3,
	GPRS_NOT_ATTACHED = -4,
	GPRS_SHUT_FAILED = -5,
	GPRS_SET_APN_FAILED = -6,
	GPRS_ACTIVE_PDP_FAILED = -7,
	GPRS_GET_IP_FAILED = -8,
	GPRS_CONNECT_FAILED = -9,
	GPRS_SEND_FAILED = -10,
	GPRS_ERROR_OTHERS = -99,
};

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
char uid[36];   /* 32 bytes uid */
bool authReqFlag = false;
bool ledToggleFlag = false;

volatile uint32_t tick_ct = 0;
volatile uint32_t systemTimer = 0;
RINGBUFF_T txring, rxring;
char rxbuff[RX_RB_SIZE], txbuff[TX_RB_SIZE];
char ATRXBuffer[AT_RX_BUF_SIZE];
const char *description = "SW Auth Demo\r\n";
char *authStr = "authorization request\r\n";

uint32_t tick_tmp;
extern void delay_ms(uint32_t time);
//uint32_t tick_tmp;
/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/

int setupGPRS(void)
{
	int signal;
	int retry;
	char ip[16];
		
	if(Air202_powerOn())
		return GPRS_POWER_ON_FAIL;
	delay_ms(3000);
	if(Air202_setEcho(0))
		return GPRS_ERROR_OTHERS;
	retry = 5;
	while(retry--){
		if(!Air202_checkPIN())
			break;	
	}
	if(retry==0)
		return GPRS_SIM_NOT_READY;
	signal = Air202_checkSignal();
	if(signal < SQ_DEADLINE)
		return GPRS_SIGNAL_POOR;
	retry = 5;
	while(retry--){
		if(Air202_checkAttach() == ATTACHED)
			break;
	}
	if(retry==0)
		return GPRS_NOT_ATTACHED;
	if(Air202_IPShut())
		return GPRS_SHUT_FAILED;
	if(Air202_setAPN(APN))
		return GPRS_SET_APN_FAILED;
	if(Air202_activePDP())
		return GPRS_ACTIVE_PDP_FAILED;
	if(Air202_checkIPAddress(ip))
		return GPRS_GET_IP_FAILED;
	DEBUGOUT("IP:%s\r\n",ip);
	return GPRS_SUCCESS;
}

int authorization(void)
{
	char txt[] = "authorization request\r\n";
	if(Air202_IPStart(TCP_PROTOCOL,SERVER_IP,SERVER_PORT))
		return GPRS_CONNECT_FAILED;
	if(Air202_IPSend(txt,strlen(txt)))
		return GPRS_SEND_FAILED;
	return RET_CODE_SUCCESS;
}


/**
 * @brief	Handle interrupt from SysTick timer
 * @return	Nothing
 */

void SysTick_Handler(void)
{
	if ((tick_ct % 1000) == 0) {
		ledToggleFlag = true;
		systemTimer++;
	}
	#if AUTH_ENABLE
	if((tick_ct % AUTH_PERIOD) == 0){
		authReqFlag = true;
	}
	#endif
	tick_ct++;
}

/**
 * @brief	Handle interrupt from UART2
 * @return	Nothing
 */
void UART2_IRQHandler(void)
{
	Chip_UART_IRQRBHandler(AT_UART,&rxring,&txring);
}


/**
 * @brief	User application
 * @return	Nothing
 */

void userApp(void)
{
	if(ledToggleFlag == true){
		ledToggleFlag = false;
		Board_LED_Toggle(LED_USED);
	}
}

int getUID(const char* uid_addr,char *uid_data)
{
	int i,crc16,checksum;
	if(uid_addr == NULL)
		return -1;
	for(i=0;i<UID_SIZE+2;i++) //extra 2 bytes checksum,Little-Endian
		uid_data[i] = *(uid_addr+i);
	crc16 = calculate_crc16(uid_data,UID_SIZE);
	checksum = uid_data[UID_SIZE]<<8 | uid_data[UID_SIZE+1];
//	DEBUGOUT("crc16:%d,checksum:%d",crc16,checksum);
	if(crc16 != checksum){
		return -2;
	}
	uid_data[UID_SIZE] = '\0';
	return 0;
}

/**
 * @brief	
 * @return
 */
static void setupUART(LPC_UART_T *pUART,uint32_t baudrate)
{
	LPC1125_IRQn_Type IRQn;
	/* pin configure */
	if(pUART == LPC_UART0){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_6, (IOCON_FUNC1 | IOCON_MODE_INACT)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_7, (IOCON_FUNC1 | IOCON_MODE_INACT)); /* TXD */	
	}else if(pUART == LPC_UART1){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_7, (IOCON_FUNC3 | IOCON_MODE_INACT)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_6, (IOCON_FUNC3 | IOCON_MODE_INACT)); /* TXD */	
	}else if(pUART == LPC_UART2){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_3, (IOCON_FUNC3 | IOCON_MODE_INACT)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_8, (IOCON_FUNC3 | IOCON_MODE_INACT)); /* TXD */	
	}else{
		return;
	}
	/* UART Configure */
	Chip_UART_Init(pUART);
	Chip_UART_SetBaud(pUART,baudrate);
	Chip_UART_ConfigData(pUART, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT | UART_LCR_PARITY_DIS));
	Chip_UART_SetupFIFOS(pUART, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));
	Chip_UART_TXEnable(pUART);
	
	/* Enable receive data and line status interrupt */
	Chip_UART_IntEnable(pUART, (UART_IER_RBRINT | UART_IER_RLSINT));

	/* preemption = 1, sub-priority = 1 */
	if(pUART == LPC_UART0){
		IRQn = UART0_IRQn;
	}else if(pUART == LPC_UART1){
		IRQn = UART1_IRQn;
	}else if(pUART == LPC_UART2){
		IRQn = UART2_IRQn;
	}
	NVIC_SetPriority(IRQn,1);
	NVIC_EnableIRQ(IRQn);
}

int AT_Send(const char *str,int size)
{
   return Chip_UART_SendRB(AT_UART,&txring,str,size);
}

int AT_Read(char *str)
{
	int n = RingBuffer_GetCount(&rxring);
	if(n<=0)
		return 0;
	else{
		return Chip_UART_ReadRB(AT_UART,&rxring,str,n);
	}
}

void GPIO_Init(void)
{
	//PIO3_3,Output high at default
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN,1); //set high as default
}

void setGPRSCtlPinStatu(bool val)
{
	Chip_GPIO_SetPinState(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN,val);
}

void test(void)
{
//	int signal;
//	int ipStatus;
//	int regStatus;
//	int attachStatus;
//	char ip[16];
	int limitSize;
	char str[] = "helloworld";
	//	signal = Air202_checkSignal();
//  if(signal>0)
//		DEBUGOUT("Get signal:%d\r\n",signal);
//	else
//		DEBUGOUT("ret:%d\r\n",signal);
//	if(!Air202_checkPIN())
//		DEBUGOUT("check PIN OK\r\n");
//	tick_tmp = tick_ct;
//	delay_ms(100);
//	DEBUGOUT("time:%d\r\n",tick_ct-tick_tmp);
//	ipStatus = Air202_getIPStatus();
//	if(ipStatus>=0)
//		DEBUGOUT("Get IP status :%d\r\n",ipStatus);
//	if(!Air202_setAPN(APN))
//		DEBUGOUT("Set APN OK\r\n");
//	if(!Air202_IPStart(TCP_PROTOCOL,SERVER_IP,SERVER_PORT))
//		DEBUGOUT("IP Start OK\r\n");
//	regStatus = Air202_CheckRegStatus();
//	if(regStatus >= 0)
//		DEBUGOUT("reg status:%d\r\n",regStatus);
//	attachStatus = Air202_checkAttach();
//	if(attachStatus >= 0)
//		DEBUGOUT("attach status:%d\r\n",attachStatus);
//	if(!Air202_checkIPAddress(ip))
//		DEBUGOUT("get ip address:%s\r\n",ip);
//		if(!Air202_IPClose())
//			DEBUGOUT("IP close OK\r\n");
//	limitSize = Air202_checkSendLimitSize();
//	if(limitSize > 0)
//		DEBUGOUT("limit size:%d\r\n",limitSize);
//	if(!Air202_IPSend(str,strlen(str)))
//		DEBUGOUT("Send OK\r\n");
//		if(!Air202_IPShut())
//			DEBUGOUT("Shut OK\r\n");
//	if(!Air202_powerOn())
//		DEBUGOUT("Power on\r\n");
//	if(!Air202_powerOff())
//		DEBUGOUT("Power off\r\n");
	while(1){
	}
}

/**
 * @brief	main routine for blinky example
 * @return	Function should not exit.
 */
int main(void)
{
	int ret;
	
	SystemCoreClockUpdate();
	Board_Init();
	GPIO_Init();

	/* Enable and setup SysTick Timer at a periodic rate */
	SysTick_Config(SystemCoreClock / TICKRATE_HZ);
  setupUART(AT_UART,AT_UART_BAUDRATE);
	RingBuffer_Init(&rxring, rxbuff, 1, RX_RB_SIZE);
	RingBuffer_Init(&txring, txbuff, 1, TX_RB_SIZE);
	
	DEBUGOUT("%s",description);
	if(!getUID((char*)UID_ADDR,uid)){
		DEBUGOUT("get uid:%s\r\n",uid);
	}else{
		DEBUGOUT("get uid failed\r\n");
	}
	
	ret = setupGPRS();
	if(!ret){
		DEBUGOUT("GPRS had been setup\r\n");
	}else{
		DEBUGOUT("GPRS Setup failed,ret=%d\r\n",ret);
		for(;;);
	}
	if(!Air202_IPStart(TCP_PROTOCOL,SERVER_IP,SERVER_PORT)){
		DEBUGOUT("Connect to server\r\n");
	}else{
		DEBUGOUT("Failed to connect to server\r\n");
		for(;;);
	}
	
	while (1){
		#if AUTH_ENABLE
		if(authReqFlag == true){
			DEBUGOUT("start to authorize!\r\n");
			authReqFlag = false;
			if(!Air202_IPSend(authStr,strlen(authStr))){
				DEBUGOUT("Send successfully\r\n");
			}else{
				DEBUGOUT("Send failed\r\n");
			}
		}
		#endif
		
		userApp();
	}
}
