/*
 * Copyright (C) 1994-2021 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of both the OpenPBS software ("OpenPBS")
 * and the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * OpenPBS is free software. You can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * OpenPBS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * PBS Pro is commercially licensed software that shares a common core with
 * the OpenPBS software.  For a copy of the commercial license terms and
 * conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
 * Altair Legal Department.
 *
 * Altair's dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of OpenPBS and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair's trademarks, including but not limited to "PBS™",
 * "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
 * subject to Altair's trademark licensing policies.
 */

#include <pbs_config.h> /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/**
 * @file	attr_fn_acl.c
 * @brief
 * 	This file contains general functions for attributes of type
 *      User/Group/Hosts Acess Control List.
 * @details
 * 	The following functions should be used for the 3 types of ACLs:
 *
 *	 User ACL	 Group ACL	 Host ACL
 *	(+ mgrs + ops)
 *	---------------------------------------------
 * 	decode_arst	decode_arst	decode_arst
 * 	encode_arst	encode_arst	encode_arst
 *	set_uacl	set_gacl	set_hostacl
 *	comp_arst	comp_arst	comp_arst
 *	free_arst	free_arst	free_arst
 *
 * 	The "encoded" or external form of the value is a string with the orginial
 * 	strings separated by commas (or new-lines) and terminated by a null.
 *
 * 	The "decoded" form is a set of strings pointed to by an array_strings struct
 *
 * 	These forms are identical to ATR_TYPE_ARST, and in fact encode_arst(),
 * 	comp_arst(), and free_arst() are used for those functions.
 *
 * 	set_ugacl() is different because of the special  sorting required.
 */

/* External Functions called */

/* Private Functions */

static int hacl_match(const char *can, const char *master);
static int gacl_match(const char *can, const char *master);
static int user_match(const char *can, const char *master);
static int sacl_match(const char *can, const char *master);
static int host_order(char *old, char *new);
static int user_order(char *old, char *new);
static int group_order(char *old, char *new);
static int
set_allacl(attribute *, attribute *, enum batch_op,
	   int (*order_func)());

/* for all decode_*acl() - use decode_arst() */
/* for all encode_*acl() - use encode_arst() */

/**
 * @brief
 * 	set_uacl - set value of one User ACL attribute to another
 *	with special sorting.
 *
 *	A=B --> set of strings in A replaced by set of strings in B
 *	A+B --> set of strings in B appended to set of strings in A
 *	A-B --> any string in B found is A is removed from A
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 *
 * @return	int
 * @retval	0	if ok
 * @retval     >0	if error
 *
 */

int
set_uacl(attribute *attr, attribute *new, enum batch_op op)
{

	return (set_allacl(attr, new, op, user_order));
}

/**
 * @brief
 * 	set_gacl - set value of one Group ACL attribute to another
 *	with special sorting.
 *
 *	A=B --> set of strings in A replaced by set of strings in B
 *	A+B --> set of strings in B appended to set of strings in A
 *	A-B --> any string in B found is A is removed from A
 *
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 *
 * @return      int
 * @retval      0       if ok
 * @retval     >0       if error
 *
 */

int
set_gacl(attribute *attr, attribute *new, enum batch_op op)
{

	return (set_allacl(attr, new, op, group_order));
}

/**
 * @brief
 * 	set_hostacl - set value of one Host ACL attribute to another
 *	with special sorting.
 *
 *	A=B --> set of strings in A replaced by set of strings in B
 *	A+B --> set of strings in B appended to set of strings in A
 *	A-B --> any string in B found is A is removed from A
 *
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 *
 * @return      int
 * @retval      0       if ok
 * @retval     >0       if error
 *
 */

int
set_hostacl(attribute *attr, attribute *new, enum batch_op op)
{

	return (set_allacl(attr, new, op, host_order));
}

/**
 * @brief
 * 	acl_check - check a name:
 *		user or [user@]full_host_name
 *		group_name
 *		full_host_name
 *	against the entries in an access control list.
 *	Match is done by calling the approprate comparison function
 *	with the name and each string from the list in turn.
 *
 * @param[in] pattr - pointer to attribute list
 * @param[in] name - acl name to be checked
 * @param[in] type - type of acl
 *
 * @return	int
 * @retval	1	if access allowed
 * @retval	0	if not allowed
 *
 */

int
acl_check(attribute *pattr, char *name, int type)
{
	int i;
#ifdef HOST_ACL_DEFAULT_ALL
	int default_rtn = 1;
#else  /* HOST_ACL_DEFAULT_ALL */
	int default_rtn = 0;
#endif /* HOST_ACL_DEFAULT_ALL */
	struct array_strings *pas;
	char *pstr;
	int (*match_func)(const char *name, const char *master);

	extern char server_host[];

	switch (type) {
		case ACL_Host:
			match_func = hacl_match;
			break;
		case ACL_User:
			match_func = user_match;
			break;
		case ACL_Group:
			match_func = gacl_match;
			break;
		case ACL_Subnet:
			match_func = sacl_match;
			break;
		default:
			match_func = (int (*)(const char *, const char *)) strcmp;
			break;
	}

	if (name == NULL)
		return (default_rtn);

	if (!(pattr->at_flags & ATR_VFLAG_SET) ||
	    ((pas = pattr->at_val.at_arst) == NULL) ||
	    (pas->as_usedptr == 0)) {

#ifdef HOST_ACL_DEFAULT_ALL
		/* no list, default to everybody is allowed */
		return (1);
#else
		if (type == ACL_Host) {
			/* if there is no list set, allow only from my host */
			return (!hacl_match(name, server_host));
		} else
			return (0);
#endif
	}

	for (i = 0; i < pas->as_usedptr; i++) {
		pstr = pas->as_string[i];
		if ((*pstr == '+') || (*pstr == '-')) {
			if (*(pstr + 1) == '\0') { /* "+" or "-" sets default */
				if (*pstr == '+')
					default_rtn = 1;
				else
					default_rtn = 0;
			}
			pstr++; /* skip over +/- if present */
		}
		if (!match_func(name, pstr)) {
			if (*pas->as_string[i] == '-')
				return (0); /* deny */
			else
				return (1); /* allow */
		}
	}
	return (default_rtn);
}

/**
 * @brief
 * 	chk_dup_acl - check for duplicate in list (array_strings)
 *
 * @param[in] old - old list of acl
 * @param[in] new - new list of acl
 *
 * @return 	int
 * @retval	0	if no duplicate
 * @retval	1	if duplicate within the new list or
 *      		between the new and old list.
 *
 */

static int
chk_dup_acl(struct array_strings *old, struct array_strings *new)
{
	int i;
	int j;

	for (i = 0; i < new->as_usedptr; ++i) {

		/* first check against self */

		for (j = 0; j < new->as_usedptr; ++j) {

			if (i != j) {
				if (strcmp(new->as_string[i], new->as_string[j]) == 0)
					return 1;
			}
		}

		/* next check new against existing (old) strings */

		for (j = 0; j < old->as_usedptr; ++j) {

			if (strcmp(new->as_string[i], old->as_string[j]) == 0)
				return 1;
		}
	}
	return 0;
}

/**
 * @brief
 * 	set_allacl - general set function for all types of acls
 *	This function is private to this file.  It is called
 *	by the public set function which is specific to the
 *	ACL type.  The public function passes an extra
 *	parameter which indicates the ACL type.
 *
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 * @param[in] order_func - function pointer to indicate action depending on acl type
 *
 * @return	int
 * @retval	PBSE error number	Failure
 * @retval	0			Success
 *
 */

static int
set_allacl(attribute *attr, attribute *new, enum batch_op op, int (*order_func)(char *, char *))
{
	int i;
	int j;
	int k;
	unsigned long nsize;
	unsigned long need;
	long offset;
	char *pc;
	char *where;
	int used;
	struct array_strings *tmppas;
	struct array_strings *pas;
	struct array_strings *newpas;
	extern void free_arst(attribute *);

	assert(attr && new && (new->at_flags &ATR_VFLAG_SET));

	pas = attr->at_val.at_arst;   /* array of strings control struct */
	newpas = new->at_val.at_arst; /* array of strings control struct */
	if (!newpas)
		return (PBSE_INTERNAL);

	if (!pas) {

		/* no array_strings control structure, make one */

		i = newpas->as_npointers;
		pas = (struct array_strings *) malloc((i - 1) * sizeof(char *) +
						      sizeof(struct array_strings));
		if (!pas)
			return (PBSE_SYSTEM);
		pas->as_npointers = i;
		pas->as_usedptr = 0;
		pas->as_bufsize = 0;
		pas->as_buf = NULL;
		pas->as_next = NULL;
		attr->at_val.at_arst = pas;
	}

	/*
	 * At this point we know we have a array_strings struct initialized
	 */

	switch (op) {
		case SET:

			/*
			 * Replace old array of strings with new array, this is
			 * simply done by deleting old strings and adding the
			 * new strings one at a time via Incr
			 */

			for (i = 0; i < pas->as_usedptr; i++)
				pas->as_string[i] = NULL; /* clear all pointers */
			pas->as_usedptr = 0;
			pas->as_next = pas->as_buf;

			if (newpas->as_usedptr == 0)
				break; /* none to set */

			nsize = newpas->as_next - newpas->as_buf; /* space needed */
			if (nsize > pas->as_bufsize) {		  /* new won't fit */
				if (pas->as_buf)
					free(pas->as_buf);
				nsize += nsize / 2; /* alloc extra space */
				if (!(pas->as_buf = malloc(nsize))) {
					pas->as_bufsize = 0;
					return (PBSE_SYSTEM);
				}
				pas->as_bufsize = nsize;

			} else { /* str does fit, clear buf */
				(void) memset(pas->as_buf, 0, pas->as_bufsize);
			}

			pas->as_next = pas->as_buf;

			/* No break, "Set" falls into "Incr" to add strings */

		case INCR:

			/* check for duplicates within new and between new and old  */

			if (chk_dup_acl(pas, newpas))
				return (PBSE_DUPLIST);

			nsize = newpas->as_next - newpas->as_buf; /* space needed */
			used = pas->as_next - pas->as_buf;	  /* space used   */

			if (nsize > (pas->as_bufsize - used)) {

				/* need to make more room for sub-strings */

				need = pas->as_bufsize + 2 * nsize; /* alloc new buf */
				if (pas->as_buf)
					pc = realloc(pas->as_buf, need);
				else
					pc = malloc(need);
				if (pc == NULL)
					return (PBSE_SYSTEM);
				offset = pc - pas->as_buf;
				pas->as_buf = pc;
				pas->as_next = pc + used;
				pas->as_bufsize = need;

				for (j = 0; j < pas->as_usedptr; j++) /* adjust points */
					pas->as_string[j] += offset;
			}

			j = pas->as_usedptr + newpas->as_usedptr;
			if (j > pas->as_npointers) {

				/* need more pointers */

				j = 3 * j / 2; /* allocate extra     */
				need = sizeof(struct array_strings) + (j - 1) * sizeof(char *);
				tmppas = (struct array_strings *) realloc((char *) pas, need);
				if (tmppas == NULL)
					return (PBSE_SYSTEM);
				tmppas->as_npointers = j;
				pas = tmppas;
				attr->at_val.at_arst = pas;
			}

			/* now put in new strings in special ugacl sorted order */

			for (i = 0; i < newpas->as_usedptr; i++) {
				for (j = 0; j < pas->as_usedptr; j++) {
					if (order_func(pas->as_string[j], newpas->as_string[i]) > 0)
						break;
				}
				/* push up rest of old strings to make room for new */

				offset = strlen(newpas->as_string[i]) + 1;
				if (j < pas->as_usedptr) {
					where = pas->as_string[j]; /* where to put in new */

					pc = pas->as_next - 1;
					while (pc >= pas->as_string[j]) { /* shift data up */
						*(pc + offset) = *pc;
						pc--;
					}
					for (k = pas->as_usedptr; k > j; k--)
						/* re adjust pointrs */
						pas->as_string[k] = pas->as_string[k - 1] + offset;
				} else {
					where = pas->as_next;
				}
				(void) strcpy(where, newpas->as_string[i]);
				pas->as_string[j] = where;
				pas->as_usedptr++;
				pas->as_next += offset;
			}
			break;

		case DECR: /* decrement (remove) string from array */
			for (j = 0; j < newpas->as_usedptr; j++) {
				for (i = 0; i < pas->as_usedptr; i++) {
					if (!strcmp(pas->as_string[i], newpas->as_string[j])) {
						/* compact buffer */
						nsize = strlen(pas->as_string[i]) + 1;
						pc = pas->as_string[i] + nsize;
						need = pas->as_next - pc;
						(void) memmove(pas->as_string[i], pc, (size_t) need);
						pas->as_next -= nsize;
						/* compact pointers */
						for (++i; i < pas->as_npointers; i++)
							pas->as_string[i - 1] = pas->as_string[i] - nsize;
						pas->as_string[i - 1] = NULL;
						pas->as_usedptr--;
						break;
					}
				}
			}
			break;

		default:
			return (PBSE_INTERNAL);
	}
	post_attr_set(attr);
	return (0);
}

/**
 * @brief
 *	user_match - User order match
 *	Match two strings by user, then from the tail end first
 *
 * @param[in] can - Canidate string (first parameter) is a single user@host string.
 * @param[in] master - Master string (2nd parameter) is an entry from a user/group acl.
 * It should have a leading + or - which is ignored.  Next is the user name
 * which is compared first.  If the user name matches, then the host name is
 * checked.  The host name may be a wild carded or null (including no '@').
 * If the hostname is null, it is treated the same as "@*", a fully wild
 * carded hostname that matches anything.
 *
 * @return	int
 * @retval	0	if string match
 * @retval	1	if not
 *
 */

static int
user_match(const char *can, const char *master)
{

	/* match user name first */

	do {
		if (*master != *can)
			return (1); /* doesn't match */
		master++;
		can++;
	} while ((*master != '@') && (*master != '\0'));

	if (*master == '\0') {
		/* if full match or if master has no host (=wildcard) */
		if ((*can == '\0') || (*can == '@'))
			return (0);
		else
			return (1);
	} else if (*can != '@')
		return (1);

	/* ok, now compare host/domain name working backwards     */
	/* if hit wild card in master ok to stop and report match */

	return (hacl_match(can + 1, master + 1));
}

/**
 * @brief
 * 	user_order - user order compare
 *
 * @param[in] s1 - user name
 * @param[in] s2 - user name
 *
 * @return	int
 * @retval	-1 	if entry s1 sorts before s2
 * @retval	0 	if equal
 * @retval	1 	if s1 sorts after s2
 *
 */

static int
user_order(char *s1, char *s2)
{
	int d;

	/* skip over the + or - prefix */

	if ((*s1 == '+') || (*s1 == '-'))
		s1++;
	if ((*s2 == '+') || (*s2 == '-'))
		s2++;

	/* compare user names first, stop with '@' */

	while (1) {
		d = (int) *s1 - (int) *s2;
		if (d)
			return (d);
		if ((*s1 == '@') || (*s1 == '\0'))
			return (host_order(s1 + 1, s2 + 1)); /* order host names */
		s1++;
		s2++;
	}
}

/**
 * @brief
 *	comapre the group names
 *
 * @param[in]   s1 - group name
 * @param[in]   s2 - group name
 *
 * @return 	int
 * @retval  	0	if equal
 * @retval 	-1	if entry s1 sorts before s2
 * @retval  	1   	if s1 sorts after s2
 *
 */
static int
group_order(char *s1, char *s2)
{

	/* skip over the + or - prefix */

	if ((*s1 == '+') || (*s1 == '-'))
		s1++;
	if ((*s2 == '+') || (*s2 == '-'))
		s2++;

	return strcmp(s1, s2);
}

/**
 * @brief
 * 	host acl order match - match two strings from the tail end first
 *
 * @param[in] can - Canidate string (first parameter) is a single user@host string.
 * @param[in] master -  Master string (2nd parameter) is an entry from a host acl.  It may have a
 * leading + or - which is ignored.  It may also have an '*' as a leading
 * name segment to be a wild card - match anything.
 *
 * Strings match if identical, or if match up to leading '*' on master which
 * like a wild card, matches any prefix string on canidate domain name
 *
 * @return	int
 * @retval	0	if strings match
 * @retval	1	if not
 *
 */

static int
hacl_match(const char *can, const char *master)
{
	const char *pc;
	const char *pm;

	pc = can + strlen(can) - 1;
	pm = master + strlen(master) - 1;
	while ((pc > can) && (pm > master)) {
		if (tolower((int) *pc) != tolower((int) *pm))
			return (1);
		pc--;
		pm--;
	}

	/* comparison of one or both reached the start of the string */

	if (pm == master) {
		if (*pm == '*')
			return (0);
		else if ((pc == can) && (tolower(*pc) == tolower(*pm)))
			return (0);
	}
	return (1);
}

/**
 * @brief
 * 	group acl order match - match two strings when user is in group
 *
 * @param[in] can - Canidate string (first parameter) is a euser string (egroup on Windows).
 * @param[in] master -  Master string (2nd parameter) is an entry from a group acl.
 *
 * Strings match if can is a member of master (strings are equal on windows).
 *
 * @return	int
 * @retval	0	if strings match
 * @retval	1	if not
 *
 */

static int
gacl_match(const char *can, const char *master)
{
#ifdef WIN32
	return (strcmp(can, master));
#else
	int i, ng = 0;
	struct passwd *pw;
	struct group *gr;
	gid_t *groups = NULL;

	if ((pw = getpwnam(can)) == NULL)
		return (1);

	if (getgrouplist(can, pw->pw_gid, NULL, &ng) < 0) {
		if ((groups = (gid_t *) malloc(ng * sizeof(gid_t))) == NULL)
			return (1);
		getgrouplist(can, pw->pw_gid, groups, &ng);
	}

	for (i = 0; i < ng; i++) {
		if ((gr = getgrgid(groups[i])) != NULL) {
			if (!strcmp(gr->gr_name, master)) {
				free(groups);
				return (0);
			}
		}
	}

	free(groups);

	return (1);
#endif
}

/**
 * @brief
 * 	subnet acl order match - match two strings: ip and subnet with mask
 *  in short or long version
 *
 * @param[in] can - Canidate string (first parameter) is a ip string.
 * @param[in] master -  Master string (2nd parameter) is an entry from a host acl.
 *
 * Strings match if ip is in subnet.
 *
 * @return	int
 * @retval	0	if strings match
 * @retval	1	if not
 *
 */

static int
sacl_match(const char *can, const char *master)
{
	struct in_addr addr;
	uint32_t ip;
	uint32_t subnet;
	uint32_t mask;
	char tmpsubnet[PBS_MAXIP_LEN + 1];
	char *delimiter;
	int len;
	int short_mask;

	/* check and convert candidate to numeric IP */
	if (inet_pton(AF_INET, can, &addr) == 0)
		return 1;
	ip = ntohl(addr.s_addr);

	/* split master to subnet and mask */
	if ((delimiter = strchr(master, '/')) == NULL)
		return 1;

	if (*(delimiter + 1) == '\0')
		return 1;

	len = delimiter - master;
	if (len > PBS_MAXIP_LEN)
		return 1;

	/* get subnet */
	strncpy(tmpsubnet, master, len);
	tmpsubnet[len] = '\0';
	if (inet_pton(AF_INET, tmpsubnet, &addr) == 0)
		return 1;
	subnet = ntohl(addr.s_addr);

	/* get mask */
	if (strchr(delimiter + 1, '.')) {
		/* long mask */
		if (inet_pton(AF_INET, delimiter + 1, &addr) == 0)
			return 1;
		mask = ntohl(addr.s_addr);
	} else {
		/* short mask */
		short_mask = atoi(delimiter + 1);
		if (short_mask < 0 || short_mask > 32)
			return 1;
		mask = short_mask ? ~0 << (32 - short_mask) : 0;
	}

	if (mask == 0)
		return 1;

	return ! ((ip & mask) == (subnet & mask));
}

/**
 * @brief
 *	host reverse order compare - compare two host entrys from the tail end first
 *	domain name segment at at time.
 *
 * @param[in] s1 - hostname
 * @param[in] s2 - hostname
 *
 * @return	int
 * @retval	-1 	if entry s1 sorts before s2
 * @retval	0 	if equal
 * @retval	1 	if s1 sorts after s2
 *
 */

static int
host_order(char *s1, char *s2)
{
	int d;
	char *p1;
	char *p2;

	if ((*s1 == '+') || (*s1 == '-'))
		s1++;
	if ((*s2 == '+') || (*s2 == '-'))
		s2++;

	p1 = s1 + strlen(s1) - 1;
	p2 = s2 + strlen(s2) - 1;
	while (1) {
		d = (int) *p2 - (int) *p1;
		if ((p1 > s1) && (p2 > s2)) {
			if (d != 0)
				return (d);
			else {
				p1--;
				p2--;
			}
		} else if ((p1 == s1) && (p2 == s2)) {
			if (*p1 == '*')
				return (1);
			else if (*p2 == '*')
				return (-1);
			else
				return (d);
		} else if (p1 == s1) {
			return (1);
		} else {
			return (-1);
		}
	}
}
