#define SOCKET_NAME "xxx"
#define MESSAGE_LEN 100
#define GW_PORT 3000
#define SRV1_PORT 3001
#define IP_ADDR "10.0.2.15"

typedef struct message_gw{
    int  type;
    char address[20];
    int  port;
} message_gw;

typedef struct message{
    int type;
    char buffer[MESSAGE_LEN];
    int  warning;
} message;
