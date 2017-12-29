#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <util.h>
#include <vector>
#include <libwebsockets.h>

#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KMAG "\033[0;35m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define RESET "\033[0m"

static int destroy_flag = 0;
ws_conn_entity_pool ws_conn_pool;

static void INT_HANDLER(int signo) {
    destroy_flag = 1;
}

/* *
 * websocket_write_back: write the string data to the destination wsi.
 */
int websocket_write_back(struct lws *wsi_in, char *str, int str_size_in)
{
    if (str == NULL || wsi_in == NULL)
        return -1;

    int n;
    int len;
    char *out = NULL;

    if (str_size_in < 1)
        len = strlen(str);
    else
        len = str_size_in;

    out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
    //* setup the buffer*/
    memcpy (out + LWS_SEND_BUFFER_PRE_PADDING, str, len );
    //* write out*/
    n = lws_write(wsi_in, (unsigned char*)(out + LWS_SEND_BUFFER_PRE_PADDING), len, LWS_WRITE_BINARY);

    //printf(KBLU"[websocket_write_back] %s\n"RESET, str);
    //* free the buffer*/
    free(out);

    return n;
}


static int ws_service_callback(
                         struct lws *wsi,
                         enum lws_callback_reasons reason, void *user,
                         void *in, size_t len)
{
		ws_conn_info tmp = {.user=user, .wsi=wsi};
		ws_conn_info *info = NULL;
		//printf("[Main Service] reason:%d user:%p \n",reason,user);
    switch (reason) {

        case LWS_CALLBACK_ESTABLISHED:

			ws_conn_pool.add(tmp);
            printf(KYEL"[Main Service] Connection established size:%d\n"RESET,ws_conn_pool.size());
            break;

        //* If receive a data from client*/
        case LWS_CALLBACK_RECEIVE:

            printf("%d\n",len );
            for(int i=0;i<len;i++){
            printf("%02x ",((char*)in)[i] );
            }
            printf("\n");

			info = ws_conn_pool.find(user);
            //printf(KCYN_L"[Main Service] Server recvived:%s size:%d\n"RESET,(char *)in,ws_conn_pool.size());
            //if(websocket_write_back(wsi ,(char *)in, -1)<0)
            {
	            lws_send_pipe_choked(wsi);
	            //* echo back to client*/
	            websocket_write_back(wsi ,(char *)in, len);
            }

            break;
        case LWS_CALLBACK_CLOSED:
			ws_conn_pool.remove(user);
            printf(KYEL"[Main Service] Client close. size:%d\n"RESET,ws_conn_pool.size());
            break;

        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            //return -1;
            break;

    default:
            break;
    }

    return 0;
}

struct per_session_data {
    int fd;
};

int main(void) {
    // server url will usd port 5000
    int port = 5000;
    const char *interface = NULL;
    struct lws_context_creation_info info;
    struct lws_protocols protocol;
    struct lws_context *context;
    // Not using ssl
    const char *cert_path = NULL;
    const char *key_path = NULL;
    // no special options
    int opts = 0;


    //* register the signal SIGINT handler */
    struct sigaction act;
    act.sa_handler = INT_HANDLER;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction( SIGINT, &act, 0);

    //* setup websocket protocol */
    protocol.name = "my-echo-protocol";
    protocol.callback = ws_service_callback;
    protocol.per_session_data_size=sizeof(struct per_session_data);
    protocol.rx_buffer_size = 0;

    //* setup websocket context info*/
    memset(&info, 0, sizeof info);
    info.port = port;
    info.iface = interface;
    info.protocols = &protocol;
    //info.extensions = lws_get_internal_extensions();
    info.ssl_cert_filepath = cert_path;
    info.ssl_private_key_filepath = key_path;
    info.gid = -1;
    info.uid = -1;
    info.options = opts;

    //* create libwebsocket context. */
    context = lws_create_context(&info);
    if (context == NULL) {
        printf(KRED"[Main] Websocket context create error.\n"RESET);
        return -1;
    }

    printf(KGRN"[Main] Websocket context create success.\n"RESET);

    //* websocket service */
    while ( !destroy_flag ) {
        lws_service(context, 500000);
    }
printf("TO END....\n");
    usleep(10);
    lws_context_destroy(context);

    return 0;
}
