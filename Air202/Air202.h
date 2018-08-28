#ifndef _AIR202_H
#define _AIR202_H

#include "chip.h"

#ifdef __cplusplus
extern "C"{
#endif

#define AT_RX_BUF_SIZE      (512)	
#define AT_TX_BUF_SIZE      (128)
	
typedef enum RET_CODE{
	RET_CODE_ERROR = -1,
	RET_CODE_SUCCESS = 0,
}RET_CODE_T;	

typedef enum IPSTATUS{
	IP_INITIAL = 0,
	IP_START,
	IP_CONFIG,
	IP_GPRSACT,
	IP_STATUS,
	IP_PROCESSING,
	PDP_DEACT,
	SL_TCP_CONNECTING,
	SL_UDP_CONNECTING,
	SL_SERVER_LISTENING,
	SL_CONNECT_OK,
	SL_TCP_CLOSING,
	SL_UDP_CLOSING,
	SL_TCP_CLOSED,
	SL_UDP_CLOSED,
	IP_STATUS_MAX,
}IPSTATUS_T;

typedef enum REG_STAT{
	REG_STAT_NOT_REGISTER = 0,
	REG_STAT_REGISTERED,
	REG_STAT_NOT_REGISTER_SEARCHING,
	REG_STAT_REFUSE,
	REG_STAT_UNKNOWN,
	REG_STAT_REGISTERED_ROAM,
}REG_STAT_T;

typedef enum REG_STAT_NOTIFY_CFG{
	NOTIFY_CFG_DISABLE = 0,
	NOTIFY_CFG_ENABLE_NOTIFY_STAT,
	NOTIFY_CFG_ENABLE_NOTIFY_STAT_CI,
}REG_STAT_NOTIFY_CFG_T;

enum ATTACH_STAT{
	ATTACHED = 1,
	NOT_ATTACHED = 0,
};

/* macro definition */	

#define    AT_OK                 "OK"
#define    AT_ERROR              "ERROR"
	
#define    AT                    "AT\r"
#define    ATE                   "ATE"
#define    AT_CHECK_SIGNAL       "AT+CSQ\r"
#define    AT_CHECK_REGISTER   	 "AT+CGREG?\r"
#define    AT_CHECK_ATTACH	     "AT+CGATT?\r"
#define    AT_CHECK_PIN          "AT+CPIN?\r"
#define    AT_CHECK_IP_STATUS    "AT+CIPSTATUS\r"
#define    AT_CHECK_IP_ADDRESS   "AT+CIFSR\r"
#define    AT_CHECK_SEND_SIZE    "AT+CIPSEND?\r"
#define    AT_ACTIVE_GPRS        "AT+CIICR\r"
#define    AT_IP_START           "AT+CIPSTART="
#define    AT_SET_APN            "AT+CSTT="
#define    AT_IP_CLOSE           "AT+IPCLOSE\r"
#define    AT_SL_SEND            "AT+CIPSEND\r"
#define    AT_IP_SHUT            "AT+CIPSHUT\r"
#define    AT_POWER_DOWN         "AT+CPOWD=1\r"
#define    AT_SEND_OK            "SEND OK"
#define    AT_SHUT_OK            "SHUT OK"
#define    AT_CONNECT_OK         "CONNECT OK"
#define    END_CODE              "\r"	

#define    AT_CHECK_SIGNAL_RESP          "+CSQ: "
#define    AT_CHECK_PIN_RESP             "+CPIN: READY"
#define    AT_CHECK_REGISTER_RESP        "+CGREG: "
#define    AT_CHECK_IPSTATUS_RESP        "STATE:"
#define    AT_CHECK_ATTACH_RESP          "+CGATT: "
#define    AT_IP_CLOSE_RESP              "CLOSE OK"
#define    AT_CHECK_SEND_SIZE_RESP       "+CIPSEND: "
#define    AT_POWER_DOWN_RESP            "NORMAL POWER DOWN"

#define    TCP_PROTOCOL           "TCP"
#define    UDP_PROTOCOL           "UDP"
#define    APN                    "CMNET"

#define    TIMEOUT_MS_1000       (1000)
#define    TIMEOUT_MS_3000       (3000)
#define    TIMEOUT_CONNECT       (10000)
#define    TIMEOUT_SEND_SLOW     (5000)

/* function declaration */	
static int sendAndGet(const char *strSend,const char* exp,uint32_t timeout_ms);
static int sendAndGetTimes(const char *strSend,const char* exp,uint32_t timeout_ms,uint8_t n);

int Air202_ATInit(void);
int Air202_checkSignal(void);
int Air202_checkPIN(void);
int Air202_setAPN(char *apn);
int Air202_checkRegStatus(void);
int Air202_checkAttach(void);
int Air202_checkIPAddress(char *local_ip);
int Air202_checkSendLimitSize(void);
int Air202_getIPStatus(void);
int Air202_activePDP(void);
int Air202_setEcho(bool setting);
int Air202_IPStart(const char *protocol,const char *ip,uint16_t port);
int Air202_IPSend(const char *data, uint16_t size);
int Air202_IPClose(void);
int Air202_IPShut(void);
int Air202_powerOn(void);
int Air202_powerOff(void);

#ifdef __cplusplus
}
#endif	

#endif
