/*
 * The Spread Toolkit.
 *     
 * The contents of this file are subject to the Spread Open-Source
 * License, Version 1.0 (the ``License''); you may not use
 * this file except in compliance with the License.  You may obtain a
 * copy of the License at:
 *
 * http://www.spread.org/license/
 *
 * or in the file ``license.txt'' found in this distribution.
 *
 * Software distributed under the License is distributed on an AS IS basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License.
 *
 * The Creators of Spread are:
 *  Yair Amir, Michal Miskin-Amir, Jonathan Stanton, John Schultz.
 *
 *  Copyright (C) 1993-2009 Spread Concepts LLC <info@spreadconcepts.com>
 *
 *  All Rights Reserved.
 *
 * Major Contributor(s):
 * ---------------
 *    Ryan Caudy           rcaudy@gmail.com - contributions to process groups.
 *    Claudiu Danilov      claudiu@acm.org - scalable wide area support.
 *    Cristina Nita-Rotaru crisn@cs.purdue.edu - group communication security.
 *    Theo Schlossnagle    jesus@omniti.com - Perl, autoconf, old skiplist.
 *    Dan Schoenblum       dansch@cnds.jhu.edu - Java interface.
 *
 */

#include "sp.h"
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG       0 

static	char	    User[80];
static  char        Spread_name[80];
static  char        Private_group[MAX_GROUP_NAME];
static  char        group[MAX_GROUP_NAME];
static  mailbox     Mbox;
static	int	        Num_sent;
static  struct      timeval start_time;
static  struct      timeval end_time;
static  int         num_processes;
static  int         process_index;
static  int         num_messages;
static  int         num_finished_processes = 0;
static  int         num_messages_received = 0;
static  FILE        *fd = NULL;

#define PAYLOAD_SIZE    1200
#define MAX_MESSLEN     1212
#define MAX_MEMBERS     10
#define BURST_SIZE_INIT 300
#define BURST_SIZE      20
#define BURST_OFFSET    0
#define MAX_GROUPS      MAX_MEMBERS
#define RAND_RANGE_MAX  1000000

/* Message: Struct for multicasted message */
typedef struct {
    int             process_index;
    int             message_index;
    int             rand;
    char            payload[PAYLOAD_SIZE];
} Message;


/* Function prototypes */
static	void	    Read_message();
static  void	    burst_message(int burst_size);
static	void	    Usage( int argc, char *argv[] );
static  void        Print_help();
static  void	    Bye();

int main( int argc, char *argv[] )
{
    /* Local vars */
    int	    ret;
    sp_time test_timeout;

    /* Set timeouts */
    test_timeout.sec = 5;
    test_timeout.usec = 0;
    
    /* Parse arguments, display usage if invalid */
    Usage(argc, argv);

    /* Connect to spread group */
    ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
    if(ret != ACCEPT_SESSION) 
    {
        SP_error(ret);
        Bye();
    }
    if (DEBUG) {
        printf("User: connected to %s with private group %s\n", Spread_name, Private_group);
    }

    /* Join group */
    ret = SP_join(Mbox, group); 
    if(ret < 0) SP_error(ret);

    /* Set up event handling and queue message bursting */
    E_init();
    E_attach_fd(Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY);
    Num_sent = 0;
    
    /* Begin event handling */
    E_handle_events();

    return(0);
}

/* Burst Messages */
static void	burst_message(int burst_size)
{
	char	        mess[MAX_MESSLEN];
	unsigned int	mess_len = MAX_MESSLEN;
	int	            ret;
    int             mess_type = 0;
	int	            i;

    if (num_messages == 0) {
        mess_type = 1;
        ((Message *)(mess))->message_index = -1;
        ret= SP_multicast(Mbox, AGREED_MESS, group, mess_type, mess_len, mess);

        if(ret < 0) 
        {
            SP_error(ret);
            Bye();
        }
    }
    for (i=0; i < burst_size && Num_sent+1 <= num_messages; i++) {
        ((Message *)(mess))->process_index = process_index;
        ((Message *)(mess))->message_index = Num_sent;
        ((Message *)(mess))->rand = (rand() % RAND_RANGE_MAX) + 1;
        /* TODO: Generate messages */
        Num_sent++;
        /* Process has sent all its messages. */
        if (Num_sent == num_messages) {
            /* Set mess_type to indicate that it is last message (process is done). */
            mess_type = 1;
        }
        ret= SP_multicast(Mbox, AGREED_MESS, group, mess_type, mess_len, mess);

        if(ret < 0) 
        {
            SP_error(ret);
            Bye();
        }

        if (DEBUG)
            printf("sent message %d (total %d)\n", i+1, Num_sent);
    }
}

/* TODO: FIX THE FIXME */
/* FIXME: The user.c code does not use memcpy()s to avoid bus errors when
 *        dereferencing a pointer into a potentially misaligned buffer */

static void	Read_message() {
    /* Local vars */
    static char	    mess[MAX_MESSLEN];
    char		    sender[MAX_GROUP_NAME];
    char		    target_groups[MAX_GROUPS][MAX_GROUP_NAME];
    membership_info memb_info;
    int		        num_groups;
    int		        service_type;
    int16		    mess_type;
    int		        endian_mismatch;
    int		        i;
    int		        ret;
    Message         *message;

    /* TODO: Poll here */

    /* TODO: Start loop here */
    service_type = 0;
	ret = SP_receive(Mbox, &service_type, sender, MAX_GROUPS, &num_groups, target_groups, 
		&mess_type, &endian_mismatch, sizeof(mess), mess);

	if (DEBUG) printf("\n============================\n");
	
    if(ret < 0) 
	{
        SP_error(ret);
		Bye();
	}

	if(Is_regular_mess(service_type)) {
		
		if (!Is_agreed_mess(service_type)) {
            perror("mcast: non-agreed service message received\n");
            Bye();
        }
		if (DEBUG) { 
            printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
			sender, mess_type, endian_mismatch, num_groups, ret, mess);
            printf("%d message received\n", num_messages_received+1);
        }
        /* Done for more readable code for writing to file. */
        message = (Message *)mess;
        if (message->message_index >= 0) {
            num_messages_received++;
            /* Write message content to file. */
            fprintf( fd, "%2d, %8d, %8d\n", message->process_index, message->message_index, message->rand);
        }

        if (mess_type == 1 && ++num_finished_processes == num_processes) {
            /* All processes have finished sending (and therefore, all are done receiving).*/
            gettimeofday(&end_time, 0);
            int total_time = (end_time.tv_sec*1e6 + end_time.tv_usec)
                - (start_time.tv_sec*1e6 + start_time.tv_usec);
            
            printf("Total measured time: %d ms\n", (int)(total_time/1e3));
            printf("Throughput: %f mbps\n\n", num_messages_received * 1216 * 8.0 / total_time);
 
            Bye();
        }
        if (Num_sent < num_messages && message->process_index == process_index 
            && (message->message_index % BURST_SIZE) == BURST_OFFSET) {
            burst_message(BURST_SIZE);
        }
	} else if(Is_membership_mess(service_type)) {
        ret = SP_get_memb_info(mess, service_type, &memb_info);
        if (ret < 0) {
                printf("BUG: membership message does not have valid body\n");
                SP_error(ret);
                Bye();
        }
		if (Is_reg_memb_mess(service_type)) {
            if (DEBUG) {
    			printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
	    			sender, num_groups, mess_type);
                for (i=0; i < num_groups; i++)
                    printf("\t%s\n", &target_groups[i][0]);
                printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2]);
            }
			if(Is_caused_join_mess(service_type)) {
                /* If everyone has joined, begin bursting messages.*/
                if (num_groups == num_processes) {
                    printf("Full group membership acheived\n");
                    gettimeofday(&start_time, 0);
                    burst_message(BURST_SIZE_INIT);
                }
				if (DEBUG) { 
                    printf("Due to the JOIN of %s\n", memb_info.changed_member);
                }
			}else if (Is_caused_leave_mess( service_type)){
				printf("Due to the LEAVE of %s\n", memb_info.changed_member);
			}else if (Is_caused_disconnect_mess(service_type)){
				printf("Due to the DISCONNECT of %s\n Will now exit mcast\n", memb_info.changed_member );
                Bye();
			}else if (Is_caused_network_mess(service_type)){
				printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
			}
		} else if(Is_transition_mess(service_type)) {
			printf("received TRANSITIONAL membership for group %s\n", sender);
		} else if( Is_caused_leave_mess( service_type)){
			printf("received membership message that left group %s\n", sender);
		} else {
            printf("received incorrecty membership message of type 0x%x\n", service_type);
        }
    } else if (Is_reject_mess(service_type)) {
		printf("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
			sender, service_type, mess_type, endian_mismatch, num_groups, ret, mess);
	} else {
        printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
    }
}

static void Usage(int argc, char *argv[])
{
	sprintf( User, "bglickm1" );
	sprintf( Spread_name, "4803");
    sprintf( group, "ebennis1"); // TODO: trolololol

    if (argc != 4) {
        Print_help();
    } else {
        num_messages    = atoi(argv[1]);    // Number of messages
        process_index   = atoi(argv[2]);    // Process index
        num_processes   = atoi(argv[3]);    // Number of processes

        /* Check number of processes */
        if(num_processes > MAX_MEMBERS) {
            perror("mcast: arguments error - too many processes\n");
            exit(0);
        }
        /* Open file writer */
        char file_name[15];
        sprintf(file_name, "%d", process_index);
        if((fd = fopen(strcat(file_name, ".out"), "w")) == NULL) {
            perror("fopen failed to open file for writing");
            exit(0);
        }
    }
}
static void Print_help()
{
    printf("Usage: mcast <num_of_messages> <process_index>\
        <num_of_processes>\n");
    exit(0);
}
static void	Bye()
{
    printf("Closing file.\n");

    if (fd != NULL) {
        fclose(fd);
        fd = NULL;
    }

	printf("\nExiting mcast.\n");
	SP_disconnect( Mbox );
	exit( 0 );
}
