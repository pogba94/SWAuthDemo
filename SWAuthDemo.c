/*
 * @brief: sorfware authorization demo
 */

/*****************************************************************************
 * Header files 
 ****************************************************************************/

#include "board.h"
#include "lib_crc16.h"
#include "string.h"
#include "Air202.h"
#include "stdlib.h"
#include "cJSON.h"

/*****************************************************************************
 * Macro definitions
 ****************************************************************************/
#define AUTH_ENABLE         (1)

#define GPRS_CTL_PORT       (3)
#define GPRS_CTL_PIN        (3)

#define LED_USED            LED_GREEN
#define AT_UART             LPC_UART2
#define AT_UART_IRQHandler  UART2_IRQHandler

#define TICKRATE_HZ         (1000)	    /* 1000 ticks per second */
#define UID_ADDR            (0xF000)
#define UID_SIZE            (32)
#define AUTH_PERIOD         (60*1000)   /* 10000 miniseconds */
#define AUTH_TIMEOUT_S      (15)   
#define AT_UART_BAUDRATE    (115200)
#define TX_RB_SIZE          (256)
#define RX_RB_SIZE          (512)
#define SQ_DEADLINE         (10)
#define SOCK_IN_BUF_SIZE    (512)
#define SOCK_OUT_BUF_SIZE   (256)

#define SERVER_IP           "orange.55555.io"
#if 1
	#define SERVER_PORT       31318  
#else
	#define SERVER_PORT       28581 
#endif

#define  AUTH_REQ_PACK      "{\"apiId\":%d,\"UID\":\"%s\"}"
#define  ATUH_API_ID        1

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
 
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
 
enum RESP_CODE{
	RESP_CODE_SUCCESS = 100,
	RESP_CODE_ERROR = 4,
};

enum AUTH_STATUS{
	AUTH_STATUS_SUCCESS,
	AUTH_STATUS_FAIL,
	AUTH_STATUS_AUTHORIZING,
}AUTH_STATUS_T;

typedef struct SOCKET_BUFFER{
	char inBuffer[SOCK_IN_BUF_SIZE];
	char outBuffer[SOCK_OUT_BUF_SIZE];
}SOCKET_BUFFER_T;

typedef struct AUTH_INFO{
	int status; 
	bool authFlag;
	bool firstAuthFlag;
	int authTime;
}AUTH_INFO_T;

bool ledToggleFlag = false;
char uid[36];   /* 32 bytes uid */
volatile uint32_t tick_ct = 0;
volatile uint32_t systemTimer = 0;
RINGBUFF_T txring, rxring;
SOCKET_BUFFER_T socketBuffer;
AUTH_INFO_T authInfo = {AUTH_STATUS_FAIL,false,true};
char rxbuff[RX_RB_SIZE], txbuff[TX_RB_SIZE];
char ATRXBuffer[AT_RX_BUF_SIZE];
const char *description = "SW Auth Demo\r\n";
char *authStr = "authorization request\r\n";

/*****************************************************************************
 * Extern functions
 ****************************************************************************/
extern void delay_ms(uint32_t time);

/*****************************************************************************
 * Functions 
 ****************************************************************************/

/**
 * @brief	Handle interrupt from SysTick timer
 * @return	Nothing
 */
void SysTick_Handler(void)
{
	if ((tick_ct % 1000) == 0) {
		systemTimer++;
		if(authInfo.status == AUTH_STATUS_AUTHORIZING){
			authInfo.authTime++;
			if(authInfo.authTime > AUTH_TIMEOUT_S){
				authInfo.status = AUTH_STATUS_FAIL;
			}
		}
	}
	if(tick_ct % 500 == 0){
		ledToggleFlag = true;
	}
	#if AUTH_ENABLE
	if((tick_ct % AUTH_PERIOD) == 0){
		authInfo.authFlag = true;
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
 * @brief	  setup GPRS module
 * @return  return 0 if read successfully ,otherwise, return nagative value
*/
int setupGPRS(void)
{
	int signal;
	int retry;
	char ip[16];
		
	if(Air202_powerOn())
		return GPRS_POWER_ON_FAIL;
	delay_ms(3000);
	retry = 5;
	while(--retry){
	if(!Air202_setEcho(0))  //Disable echo
		break;
	}
	if(retry==0)
		return GPRS_ERROR_OTHERS;
	retry = 5;
	while(--retry){
	if(!Air202_setIPHead(1)) //set ip head for recieving data
		break;
	}
	if(retry == 0)
		return GPRS_ERROR_OTHERS;
	retry = 5;
	while(--retry){
		if(!Air202_checkPIN())
			break;	
	}
	if(retry==0)
		return GPRS_SIM_NOT_READY;
	signal = Air202_checkSignal();
	if(signal < SQ_DEADLINE)
		return GPRS_SIGNAL_POOR;
	retry = 5;
	while(--retry){
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

/**
 * @brief	
 * @return	
 */
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
 * @brief	  User application
 * @return	Nothing
 */

void userApp(void)
{
	if(ledToggleFlag == true){
		ledToggleFlag = false;
		Board_LED_Toggle(LED_USED);
	}
}

/**
 * @brief	 Read UID from flash
 * @return return 0 if read successfully ,otherwise, return nagative value
 */

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
 * @brief	  setup specified UART
 * @return  nothing
 */
static void setupUART(LPC_UART_T *pUART,uint32_t baudrate)
{
	LPC1125_IRQn_Type IRQn;
	/* pin configure */
	if(pUART == LPC_UART0){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_6, (IOCON_FUNC1 | IOCON_MODE_PULLUP)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_7, (IOCON_FUNC1 | IOCON_MODE_PULLUP)); /* TXD */	
	}else if(pUART == LPC_UART1){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_7, (IOCON_FUNC3 | IOCON_MODE_PULLUP)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_6, (IOCON_FUNC3 | IOCON_MODE_PULLUP)); /* TXD */	
	}else if(pUART == LPC_UART2){
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO0_3, (IOCON_FUNC3 | IOCON_MODE_PULLUP)); /* RXD */
		Chip_IOCON_PinMuxSet(LPC_IOCON, IOCON_PIO1_8, (IOCON_FUNC3 | IOCON_MODE_PULLUP)); /* TXD */	
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

/**
 * @brief	  send data to ringbuffer
 * @return  bytes sent actually
 */
int AT_Send(const char *str,int size)
{
   return Chip_UART_SendRB(AT_UART,&txring,str,size);
}

/**
 * @brief	  read out the recieved data form ringbuffer
 * @return  bytes recieved actually
 */

int AT_Read(char *str)
{
	int n = RingBuffer_GetCount(&rxring);
	if(n<=0)
		return 0;
	else{
		return Chip_UART_ReadRB(AT_UART,&rxring,str,n);
	}
}

/**
 * @brief	  initialize GPIO
 * @return  nothing
 */

void GPIO_Init(void)
{
	//PIO3_3,Output high at default
	Chip_GPIO_SetPinDIROutput(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN,1); //set high as default
}

/**
 * @brief	  setting the output level of specified pin 
 * @return  nothing
 */

void setGPRSCtlPinStatu(bool val)
{
	Chip_GPIO_SetPinState(LPC_GPIO,GPRS_CTL_PORT,GPRS_CTL_PIN,val);
}

/**
 * @brief	  check if recieved data from server
 * @return  return 0 if recieved data from server,otherwise,return nagative value
 */
int checkSockRecvData(void)
{
	int dataBytes = 0; //size of recieved 
	int n,cnt;
	char *pIPHead,*pData;
	memset(ATRXBuffer,0,sizeof(ATRXBuffer)-1);
	n = AT_Read(ATRXBuffer);
	if(n<=0) //no data
		return -1;
	delay_ms(2);//wait for recieving data
	n += AT_Read(ATRXBuffer+n);

	pIPHead = strstr(ATRXBuffer,AT_IP_HEAD);
	if(pIPHead == NULL)
		return -2;
	
	pData = pIPHead;
	pData += strlen(AT_IP_HEAD);
	dataBytes = atoi(pData);
	while(*pData++ != ':'){}
	cnt = 30;
	while((n-(pData-pIPHead)<dataBytes) && cnt--){//extra data isn't recieved
			n += AT_Read(ATRXBuffer+n);
			delay_ms(1);
	}
	if(n-(pData-pIPHead)<dataBytes){
//		DEBUGOUT("n=%d,pHead=%x,pData=%x,cnt=%d\r\n",n,(int)pIPHead,(int)pData,cnt);
		return -3;
	}
	memset(socketBuffer.inBuffer,0x0,sizeof(socketBuffer.inBuffer));
	memcpy(socketBuffer.inBuffer,pData,dataBytes);
	return dataBytes;
}

/**
 * @brief	  parse recieved data
 * @return  nothing
 */
void parseRecvData(const char *txt)
{
	int respCode;
	int apiId;
	cJSON *json = cJSON_Parse(txt);
	if(!json){
		DEBUGOUT("no json format\r\n");
		return;
	}
	if(cJSON_GetObjectItem(json,"apiId") == NULL || cJSON_GetObjectItem(json,"respCode") == NULL){
		DEBUGOUT("lack of item!\r\n");
		return;
	}
	apiId = cJSON_GetObjectItem(json,"apiId")->valueint;
	respCode = cJSON_GetObjectItem(json,"respCode")->valueint;
	DEBUGOUT("apiId:%d,respCode:%d\r\n",apiId,respCode);
	if(apiId == ATUH_API_ID && respCode == RESP_CODE_SUCCESS){
		if(authInfo.status == AUTH_STATUS_AUTHORIZING && authInfo.authTime < AUTH_TIMEOUT_S){
			authInfo.status = AUTH_STATUS_SUCCESS;
			if(authInfo.firstAuthFlag == true)
				authInfo.firstAuthFlag = false;
			DEBUGOUT("Authorization pass\r\n");
		}else{
			DEBUGOUT("Authorization error\r\n");
		}
	}else{
		authInfo.status = AUTH_STATUS_FAIL;
		DEBUGOUT("Authorization failed\r\n");
	}
	cJSON_Delete(json); //take care!
}


/**
 * @brief	  test function
 * @return  nothing
 */
void test(void)
{
//	int signal;
//	int ipStatus;
//	int regStatus;
//	int attachStatus;
//	char ip[16];
	int limitSize;
	char str[] = "helloworld";
	if(!Air202_ATInit())
		DEBUGOUT("PASS\r\n");
	else
		DEBUGOUT("FAILED\r\n");
	
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
 * @brief	  main function
 * @return	should be never return
 */
int main(void)
{
	int ret;
	int size;
	
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
	
//	test();
	
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
		if(authInfo.authFlag == true){
			DEBUGOUT("Authorizing...!\r\n");
			authInfo.authFlag = false;
			authInfo.status = AUTH_STATUS_AUTHORIZING;
			authInfo.authTime = 0;
			sprintf(socketBuffer.outBuffer,AUTH_REQ_PACK,ATUH_API_ID,uid);
			if(!Air202_IPSend(socketBuffer.outBuffer,strlen(socketBuffer.outBuffer))){
				DEBUGOUT("Send: %s\r\n",socketBuffer.outBuffer);
			}else{
				DEBUGOUT("Send failed\r\n");
			}
		}
		#endif
		
		size = checkSockRecvData();
		if(size >0){
			DEBUGOUT("recieved %d bytes,%s\r\n",size,socketBuffer.inBuffer);
			parseRecvData(socketBuffer.inBuffer);
		}else{
			if(size==-3)
				DEBUGOUT("Recieved error!\r\n");
		}
		
		if(authInfo.status != AUTH_STATUS_FAIL && authInfo.firstAuthFlag == false){
			userApp();
		}
	}
	return 0;
}
