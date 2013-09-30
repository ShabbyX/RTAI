/*
COPYRIGHT (C) 2003  Paolo Mantegazza <mantegazza@aero.polimi.it>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <net/ip.h>

#include <rtai_netrpc.h>
//#include "/home/rtai-cvs/kilauea/rtai-core/ipc/netrpc/rtnet/rtnetP.h"
#include "scrambler.h"

MODULE_LICENSE("GPL");

static long Key = 0xBEBADBED;
MODULE_PARM(Key, "i");

/*
 * what must be taken from net_rpc.c
 */

struct portslot_t { struct portslot_t *p; int indx, socket[2], task, hard; unsigned long long owner; SEM sem; void *msg; struct sockaddr_in addr; MBX *mbx; unsigned long name; };

extern void set_netrpc_encoding(void *encode, void *decode, void *ext);

extern struct rt_fun_entry **rt_net_rpc_fun_hook;

/*
 * end of what must be taken from net_rpc.c
 */

static char *type[] = { "PRT_SRV", 0, "PRT_RCV", 0, "RPC_SRV", 0, "RPC_RCV" };
static int funindx[MAX_STUBS + MAX_SOCKS];

/*
 * return values in case of error
 */
static unsigned long long reterr[] = {
	0, 0, 0, 0,  //  some addr handles
	0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL,  // getting time
	-1, -1, -1, -1,  //  generic returns
	0, -1,  // sem init/delete
	SEM_ERR, SEM_ERR, SEM_ERR, SEM_ERR, SEM_ERR, SEM_ERR,  // sem services
	0, 0, 0, 0, 0, 0, 0, 0,  // short intertask messages
	-1,  // isrpc
	0, -1,  // mbx init/delete
	0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL,
	0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL,
	0xFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFLL,  // mbx services
	0, 0, 0, 0, 0, 0, 0, 0,  // extended intertask messages
	0xFFFFFFFFFFFFLL  // a further mbx service
};

/*
 * our demo error function that does call the original function
 * (not in case of error :-)
 */
static long long errfun(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
	return  ((long long (*)(int, ...))(rt_whoami())->fun_args[9])(a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

/*
 * enquiring for a netrpc proper error: a positive return confirms a net_rpc
 * error, decremented by one indexes into the call table of net_rpc, thus
 * letting we know the API that went wrong; using zero for "port" it is
 * possible to enquire on errors for ports requests also, in which case a
 * negative value is returned
 */
int rt_netrpc_errenq(int port)
{
	return funindx[port];
}

/*
 * here is our own net_rpc calls table extension
 */
static struct rt_fun_entry rt_net_rpc_errfun[] = { { 0, errfun },
						   { 0, rt_netrpc_errenq } };
/*
 * macro to translate size in bytes to size in long
 */
#define XTOLONGSIZE(size)  ((size + sizeof(long) - 1)/sizeof(long))

/*
 * "man 3 memfrob" explains this trivial and unsafe encode/decode method.
 *  Here it is used on four bytes at a time so it might be a bit better.
 *  It remains a very simple example on how to setup NETRPC safety support
 *  however. So you should not use it for anything else but this demo.
 */
static inline void memfrob(long *msg, int size)
{
	int i;
	for (i = 0; i < XTOLONGSIZE(size); i++) {
		msg[i] ^= Key;
	}
}

/*
 * our own checksum service, maybe you'll like a crc instead
 */
static inline unsigned long chksum(long *msg, int size, unsigned long *previous)
{
	unsigned long i, s;
	if (previous) {
		*previous = msg[XTOLONGSIZE(size)];
	}
	for (i = s = 0; i < XTOLONGSIZE(size); i++) {
		s += msg[i];
	}
	return msg[XTOLONGSIZE(size)] = s;
}

/*
 * a sample encodeing service
 */
static int encode(struct portslot_t *portslotp, long *msg, int size, int where)
{
/*
 * encodeing is done on locally built messages; so no error should be in them;
 * we need just to checksum and remember the function request; in case of an
 * error on the net we can use this index to return an appropriate value if
 * and when enquired, so: ...
 */
	if (where == PRT_REQ || where == PRT_RTR) {
		chksum(msg, size - sizeof(long), 0);
/*
 * ... for a port request simply mark it with a negative value
 */
		funindx[0] = -1;
	} else { /* if (where == RPC_REQ || where == RPC_RTR) { */
		msg[XTOLONGSIZE(size)] = size;
		size = XTOLONGSIZE(size)*sizeof(long) + sizeof(long);
		chksum(msg, size, 0);
		size += sizeof(long);
/*
 * ... for a true rpc service, but not if it is a function of this module
 */
		if (!EXT(msg[4])) {
/*
 * save the index in the fuction calls table, incremented by 1 to distinguish
 * the zero indexed function from nothing
 */
			funindx[portslotp->indx] = FUN(msg[4]) + 1;
		}
	}
/*
 * now scramble it, maybe you want to compress it also
 */
	memfrob(msg, size);
	return size;
}

/*
 * a sample decodeing service
 */
static int decode(struct portslot_t *portslotp, long *msg, int size, int where)
{
	unsigned long there_is_an_error, new, old;
/*
 * unscramble it, doing whatever else needed in case you it was compressed also
 */
	memfrob(msg, size);
/*
 * revert checksum
 */
	new = chksum(msg, size - sizeof(long), &old);
	if (where == RPC_SRV || where == RPC_RCV) {
		size = msg[size/sizeof(long) - 2];
	}
/*
 * look if the checksum is OK
 */
	if ((there_is_an_error = new != old ? 1 : 0)) {
		rt_printk("CHECKSUM ERROR-> SOCK: %d-%d, %s, NEW-OLD: %lx-%lx\n", portslotp->indx, portslotp->socket[portslotp->hard], type[where - 2], new, old);
	}
/*
 * now look for other errors, add/or them to what there_is_an_error already
 */
	switch (where) {
/*
 * there is little we can do in case of errors on a port request/receive, but
 * returning a negative port to let the user know something is wrong
 */
		case PRT_SRV: {
/*
 *			if (there_is_an_error) {
 *				msg[1] = -1;
 * 			}
 */
			break;
		}
/*
 * a port request has returned with whatever error, we could inform the
 * user as below; in our case it is commented and we do nothing because the
 * execution was forced to be OK anyhow
 */
		case PRT_RCV: {
/*
 *			if (there_is_an_error) {
 *				msg[1] = -1;
 *			} else {
 *				funindx[0] = 0;
 *			}
 */
			break;
		}
/*
 * in case of a wrong msg we can ask the execution of a function of ours
 * the related msg build up can be copied from rt_net_rpc; here we execute
 * just the original request through our function (memorising it in an unused
 * arg) ...
 */
		case RPC_SRV: {
/*
 *			if (there_is_an_error) {
 */
				((RT_TASK *)portslotp->task)->fun_args[9] = (int)rt_net_rpc_fun_hook[0][FUN(msg[4])].fun;
/*
 * ... but not if it is a function of this module
 */
			if (!EXT(msg[4])) {
				msg[4] = PACKPORT(PORT(msg[4]), 1, 0, TIMED(msg[4]));
			}
/*
 *			}
 */
		}
/*
 * an rpc request has returned with whatever error, we could inform the
 * user as below; in our case it is commented and we do nothing because the
 * execution was forced to be OK anyhow
 */
		case RPC_RCV: {
/*
 *			if (there_is_an_error) {
 *				((long long *)&msg[2])[0] = reterr[funindx[portslotp->indx] - 1];
 *			} else {
 *				funindx[portslotp->indx] = 0;
 *			}
 */
			break;
		}
	}
	return size;
}

int init_module(void)
{
	set_netrpc_encoding(encode, decode, rt_net_rpc_errfun);
	printk("*** SCRAMBLER RUNNING (%lx) ***\n", Key);
	return 0;
}

void cleanup_module(void)
{
	set_netrpc_encoding(NULL, NULL, NULL);
	printk("*** SCRAMBLER REMOVED ***\n");
}
