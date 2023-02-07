#define REMOTE_PORT_DEFAULT 33333
#ifndef WIN32
#define __declspec(x)
#define SOCKET int
#endif

typedef struct Remote_Client_Status {
    int overr;
    }remote_client_status;

typedef struct Remote_Client {
    SOCKET id_sck_cli;
    int rent;
    #ifndef lib
    __declspec(dllimport)void * event_callback;
    __declspec(dllimport)void * disconnect_callback;
    #else
    void * event_callback;
    void * disconnect_callback;
    #endif
} Remote_Client;

int remote_init(char *addresse,Remote_Client * remote);
int remote_init_port(char *addresse,int port,Remote_Client* remote);
void remote_write(Remote_Client * remote,char c,int val);
int  remote_read(Remote_Client * remote,char c);
void remote_status(Remote_Client * remote, remote_client_status *stat);
void remote_close(Remote_Client * remote);
void remote_send(Remote_Client * remote,unsigned char c,unsigned char mode,short val);
void remote_receive(Remote_Client * remote,unsigned char *c,unsigned char * mode, short *val );
