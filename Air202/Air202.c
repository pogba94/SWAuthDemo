#include "Air202.h"
#include "chip.h"
#include "string.h"
#include "stdlib.h"
#include "board.h"

extern int AT_Send(const char *str,int size);
extern int AT_Read(char *str);
extern void setGPRSCtlPinStatu(bool val);
extern char ATRXBuffer[];
extern uint32_t tick_ct;

const char* IPStatusList[IP_STATUS_MAX] = {"IP INITIAL","IP START","IP CONFIG","IP GPRSACT","IP STATUS","IP PROCESSING",
				"PDP DEACT","TCP CONNECTING","UDP CONNECTING","SERVER LISTENING","CONNECT OK","TCP CLOSING",
				"UDP CLOSING","TCP CLOSED","UDP CLOSED"};

char ATTXBuffer[AT_TX_BUF_SIZE];

void delay_ms(uint32_t t)
{
	int i;
	while(t--){
		for(i=4800;i>0;i--)
			__NOP();
	}
}

void delay_us(uint32_t t)
{
	int i;
	while(t--){
		for(i=5;i>0;i--)
			__NOP();
	}
}

/**
* @brief send string,if recieved expected string in the limit time set by parameters 
				 timeout_ms ,return 0,otherwise return negative value
**/
int sendAndGet(const char *strSend,const char* exp,uint32_t timeout_ms)
{
	int n = 0;
	int time = 0;
	int byte = 0;
	char *p = NULL;
	
	if(strSend == NULL || exp == NULL)
		return -1;
	//send 
	memset(ATRXBuffer,0x0,AT_RX_BUF_SIZE);
	AT_Send(strSend,strlen(strSend));
	while(time<timeout_ms){
		n = AT_Read(ATRXBuffer + byte);
		if(n>0){
			time = 0;
			byte += n;
		}else{
			time++;
		}
		delay_ms(1);
	}
	DEBUGOUT("recv:%s\r\n",ATRXBuffer);
	p = strstr(ATRXBuffer,exp);
	if(p==NULL){
		return -2;
	}
	return 0;
}

/**
* @brief send string,if recieved expected string in the limit time set by parameters 
				 timeout_ms ,return 0,otherwise return negative value,retry n times if failed
**/
int sendAndGetTimes(const char *strSend,const char* exp,uint32_t timeout_ms,uint8_t n)
{
	int ret;
	while(n--){
		ret = sendAndGet(strSend,exp,timeout_ms);
		if(!ret){
			return ret;
		}
	}
	return ret;
}

int Air202_ATInit(void)
{
	return sendAndGet(AT,AT_OK,TIMEOUT_MS_1000);
}

int Air202_checkSignal(void)
{
	char *p;
	if(sendAndGet(AT_CHECK_SIGNAL,AT_OK,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,AT_CHECK_SIGNAL_RESP);
	if(p == NULL)
		return RET_CODE_ERROR;
	return atoi(p+strlen(AT_CHECK_SIGNAL_RESP));
}

int Air202_checkPIN(void)
{
	return sendAndGet(AT_CHECK_PIN,AT_CHECK_PIN_RESP,TIMEOUT_MS_1000);
}

/**
* @brief get ip stuatus
*/

int Air202_getIPStatus(void)
{
	int i;
	char *p;
	if(sendAndGet(AT_CHECK_IP_STATUS,AT_OK,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,AT_CHECK_IPSTATUS_RESP);
	if(p == NULL)
		return RET_CODE_ERROR;
	for(i=0;i<IP_STATUS_MAX;i++){
		p = strstr(ATRXBuffer,IPStatusList[i]);
		if(p != NULL)
			return i;
	}
	return RET_CODE_ERROR;
}

int Air202_setAPN(char *apn)
{
	if(apn == NULL)
		return RET_CODE_ERROR;
	sprintf(ATTXBuffer,"%s\"%s\"\r",AT_SET_APN,apn);
	return sendAndGet(ATTXBuffer,AT_OK,TIMEOUT_MS_1000);
}

int Air202_setEcho(bool setting)
{
	sprintf(ATTXBuffer,"%s%d\r",ATE,setting);
	return sendAndGet(ATTXBuffer,AT_OK,TIMEOUT_MS_1000);
}

int Air202_setIPHead(bool setting)
{
	sprintf(ATTXBuffer,"%s%d\r",AT_SET_IP_HEAD,setting);
	return sendAndGet(ATTXBuffer,AT_OK,TIMEOUT_MS_1000);
}

int Air202_IPStart(const char *protocol,const char *ip,uint16_t port)
{
	char *p;
	
	if(protocol == NULL || ip == NULL )
		return RET_CODE_ERROR;
	sprintf(ATTXBuffer,"%s\"%s\",\"%s\",%d\r",AT_IP_START,protocol,ip,port);
	if(sendAndGet(ATTXBuffer,AT_OK,TIMEOUT_CONNECT))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,IPStatusList[SL_CONNECT_OK]);
	return (p!=NULL)?RET_CODE_SUCCESS:RET_CODE_ERROR;
}

int Air202_IPClose(void)
{
	return sendAndGet(AT_IP_CLOSE,AT_IP_CLOSE_RESP,TIMEOUT_MS_1000);
}


int Air202_CheckRegStatus(void)
{
	char *p;
	if(sendAndGet(AT_CHECK_REGISTER,AT_OK,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,AT_CHECK_REGISTER_RESP);
	if(p == NULL)
		return RET_CODE_ERROR;
	return atoi(p + strlen(AT_CHECK_REGISTER_RESP) + 2);
}

int Air202_checkAttach(void)
{
	char *p;
	if(sendAndGet(AT_CHECK_ATTACH,AT_OK,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,AT_CHECK_ATTACH_RESP);
	if(p==NULL)
		return RET_CODE_ERROR;
	return atoi(p + strlen(AT_CHECK_ATTACH_RESP));
}

int Air202_activePDP(void)
{
	return sendAndGet(AT_ACTIVE_GPRS,AT_OK,TIMEOUT_MS_3000);
}

int Air202_checkIPAddress(char *local_ip)
{
	int p=0;
	int size=0;
	if(local_ip == NULL)
		return RET_CODE_ERROR;
	if(sendAndGet(AT_CHECK_IP_ADDRESS,END_CODE,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	while(ATRXBuffer[p] != 0){
		if((ATRXBuffer[p] >= 48 && ATRXBuffer[p] <= 57) || ATRXBuffer[p] == 46){
			local_ip[size] = ATRXBuffer[p];
			size++;
		}
//		DEBUGOUT("%c",ATRXBuffer[p]);
		p++;
	}
	if(size<=0)
		return RET_CODE_ERROR;
	local_ip[size] = '\0';
	return RET_CODE_SUCCESS;
}

int Air202_checkSendLimitSize(void)
{
	char *p;
	if(sendAndGet(AT_CHECK_SEND_SIZE,AT_OK,TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	p = strstr(ATRXBuffer,AT_CHECK_SEND_SIZE_RESP);
	if(p == NULL)
		return RET_CODE_ERROR;
	return atoi(p + strlen(AT_CHECK_SEND_SIZE_RESP));
}

int Air202_IPSend(const char *data, uint16_t size)
{
	int p = 0;
	int i =0;
	if(data == NULL)
		return RET_CODE_ERROR;
	if(sendAndGet(AT_SL_SEND,">",TIMEOUT_MS_1000))
		return RET_CODE_ERROR;
	for(i=0;i<size;i++)
		ATTXBuffer[i] = data[i];
	ATTXBuffer[size] = 0x1A; //end character
	ATTXBuffer[size+1] = '\0';
	return sendAndGet(ATTXBuffer,AT_SEND_OK,TIMEOUT_MS_3000);
}

int Air202_IPShut(void)
{
	return sendAndGet(AT_IP_SHUT,AT_SHUT_OK,TIMEOUT_MS_3000);
}

int Air202_powerOn(void)
{
	int retry = 5;
	//check if module is power on
	if(!Air202_ATInit())
		return RET_CODE_SUCCESS;
	setGPRSCtlPinStatu(0);
	delay_ms(2000);
	while(retry-->0){
		if(!Air202_ATInit()){
			setGPRSCtlPinStatu(1);
			return RET_CODE_SUCCESS;
		}
	}
	setGPRSCtlPinStatu(1);
	return RET_CODE_ERROR;
}

int Air202_powerOff(void)
{
	return sendAndGet(AT_POWER_DOWN,AT_POWER_DOWN_RESP,TIMEOUT_MS_1000);
}
