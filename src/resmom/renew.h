#ifndef RENEW_H
#define RENEW_H

#include "list_link.h"
#include "pbs_ifl.h"
#include "attribute.h"
#include "job.h"
#include "mom_mach.h"
#include "pbsgss.h"

#ifdef __cplusplus
extern "C" {
#endif

struct krb_holder;

#if defined(PBS_SECURITY) && (PBS_SECURITY == KRB5)

struct krb_holder *alloc_ticket();
int init_ticket_from_job(job *pjob, const task *ptask, struct krb_holder *ticket);
int init_ticket_from_req(char *principal, char *jobid, struct krb_holder *ticket);
int free_ticket(struct krb_holder *ticket);
char *get_ticket_ccname(struct krb_holder *ticket);
int got_ticket(struct krb_holder *ticket);

int start_renewal(const task *ptask, int, int);
int stop_renewal(const task *ptask);

#endif

#ifdef __cplusplus
}
#endif

#endif /* RENEW_H */

