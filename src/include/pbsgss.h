#ifndef _PBSGSS_H_
#define _PBSGSS_H_

#include <gssapi.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern gss_buffer_t empty_token;

void pbsgss_display_status(const char *msg, OM_uint32 maj_stat, OM_uint32 min_stat);
int pbsgss_can_get_creds();
int pbsgss_server_establish_context(int s, gss_cred_id_t server_creds, gss_cred_id_t *client_creds, gss_ctx_id_t *context, gss_buffer_t client_name, OM_uint32 *ret_flags);
int pbsgss_server_acquire_creds(char *service_name, gss_cred_id_t *server_creds);
int pbsgss_save_sec_context(gss_ctx_id_t *context, OM_uint32 flags, int fd);
const char *pbsgss_get_host_princname();
int pbsgss_client_authenticate(char *hostname, int psock, int delegate, int wrap);

#ifdef __cplusplus
}
#endif

/* Token types */
#define TOKEN_NOOP              (1<<0)
#define TOKEN_CONTEXT           (1<<1)
#define TOKEN_DATA              (1<<2)
#define TOKEN_MIC               (1<<3)

/* Token flags */
#define TOKEN_CONTEXT_NEXT      (1<<4)
#define TOKEN_WRAPPED           (1<<5)
#define TOKEN_ENCRYPTED         (1<<6)
#define TOKEN_SEND_MIC          (1<<7)


enum PBSGSS_ERRORS {
    PBSGSS_OK = 0,
    PBSGSS_ERR_IMPORT_NAME,
    PBSGSS_ERR_ACQUIRE_CREDS,
    PBSGSS_ERR_INTERNAL,
    PBSGSS_ERR_WRAPSIZE,
    PBSGSS_ERR_CONTEXT_DELETE,
    PBSGSS_ERR_CONTEXT_SAVE,
    PBSGSS_ERR_IMPORT,
    PBSGSS_ERR_IMPORTNAME,
    PBSGSS_ERR_CONTEXT_INIT,
    PBSGSS_ERR_READ = 10,
    PBSGSS_ERR_READ_TEMP,
    PBSGSS_ERR_SENDTOKEN,
    PBSGSS_ERR_RECVTOKEN,
    PBSGSS_ERR_ACCEPT_TOKEN,
    PBSGSS_ERR_NAME_CONVERT,
    PBSGSS_ERR_NO_KRB_PRINC,
    PBSGSS_ERR_NO_USERNAME,
    PBSGSS_ERR_USER_NOT_FOUND,
    PBSGSS_ERR_CANT_OPEN_FILE,
    PBSGSS_ERR_KILL_RENEWAL_PROCESS = 20,
    PBSGSS_ERR_GET_CREDS,
    PBSGSS_ERR_FLOOR
};

#endif

