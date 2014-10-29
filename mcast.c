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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define int32u      unsigned int
#define DEBUG       1

static	char	    User[80];
static  char        Spread_name[80];
static  char        Private_group[MAX_GROUP_NAME];
static  char        group[MAX_GROUP_NAME];
static  mailbox     Mbox;
static	int	        Num_sent;
static  int         To_exit = 0; // TODO: WAT DO?

#define MAX_MESSLEN 1212
#define MAX_MEMBERS 10
#define BURST_SIZE  100
/* Function prototypes */
static	void	    Read_message();
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
	    Usage( argc, argv );

        /* Connect to spread group */
	    ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
	    if( ret != ACCEPT_SESSION ) 
	    {
		    SP_error( ret );
		    Bye();
	    }
	        printf("User: connected to %s with private group %s\n", Spread_name, Private_group );
        
        /* Join group */
        ret = SP_join( Mbox, group );
        if( ret < 0 ) SP_error( ret );

        /* Set up event handling and queue message bursting */
	    E_init();
	    E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );
	    /* TODO: Enqueue message bursting if messages to send > 0 */
        Num_sent = 0;
        
        /* Begin event handling */
	    E_handle_events();

	    return( 0 );
}

/* Burst Messages */
static void	burst_message()
{
	char	        mess[MAX_MESSLEN];
	int	            num_groups;
	unsigned int	mess_len;
	int	            ret;
	int	            i;

    /* TODO: Stop if we've sent all we need to send */
    for( i=0; i < BURST_SIZE; i++ )
    {
        Num_sent++;
        /* TODO: Generate messages */
        ret= SP_multicast( Mbox, FIFO_MESS, group, 2, mess_len, mess );

        if( ret < 0 ) 
        {
            SP_error( ret );
            Bye();
        }
        if (DEBUG)
            printf("sent message %d (total %d)\n", i+1, Num_sent );
    }
}

/* TODO: FIX THE FIXME */
/* FIXME: The user.c code does not use memcpy()s to avoid bus errors when
 *        dereferencing a pointer into a potentially misaligned buffer */

static	void	Read_message() {
    /* Local vars */
    static char	    mess[MAX_MESSLEN];
    char		    sender[MAX_GROUP_NAME];
    char		    target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    membership_info memb_info;
    int		        num_groups;
    int		        service_type;
    int16		    mess_type;
    int		        endian_mismatch;
    int		        i,j;
    int		        ret;

    service_type = 0;
	ret = SP_receive( Mbox, &service_type, sender, 100, &num_groups, target_groups, 
		&mess_type, &endian_mismatch, sizeof(mess), mess );
	printf("\n============================\n");
	
    if( ret < 0 ) 
	{
        SP_error( ret );
		Bye();
	}

	if( Is_regular_mess( service_type ) )
	{
		mess[ret] = 0;
		if (!Is_agreed_mess(service_type)){
            perror("mcast: non-agreed service message received\n");
            Bye();
        }
		if (DEBUG)  
            printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
			sender, mess_type, endian_mismatch, num_groups, ret, mess );
	}else if( Is_membership_mess( service_type ) )
        {
                ret = SP_get_memb_info( mess, service_type, &memb_info );
                if (ret < 0) {
                        printf("BUG: membership message does not have valid body\n");
                        SP_error( ret );
                        Bye();
                }
		if     ( Is_reg_memb_mess( service_type ) )
		{
			printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
				sender, num_groups, mess_type );
			for( i=0; i < num_groups; i++ )
				printf("\t%s\n", &target_groups[i][0] );
			printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );

			if( Is_caused_join_mess( service_type ) )
			{
				printf("Due to the JOIN of %s\n", memb_info.changed_member );
			}else if( Is_caused_leave_mess( service_type ) ){
				printf("Due to the LEAVE of %s\n", memb_info.changed_member );
			}else if( Is_caused_disconnect_mess( service_type ) ){
				printf("Due to the DISCONNECT of %s\n", memb_info.changed_member );
			}else if( Is_caused_network_mess( service_type ) ){
				printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
                                num_vs_sets = SP_get_vs_sets_info( mess, &vssets[0], MAX_VSSETS, &my_vsset_index );
                                if (num_vs_sets < 0) {
                                        printf("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n", MAX_VSSETS);
                                        SP_error( num_vs_sets );
                                        exit( 1 );
                                }
                                for( i = 0; i < num_vs_sets; i++ )
                                {
                                        printf("%s VS set %d has %u members:\n",
                                               (i  == my_vsset_index) ?
                                               ("LOCAL") : ("OTHER"), i, vssets[i].num_members );
                                        ret = SP_get_vs_set_members(mess, &vssets[i], members, MAX_MEMBERS);
                                        if (ret < 0) {
                                                printf("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
                                                SP_error( ret );
                                                exit( 1 );
                                        }
                                        for( j = 0; j < vssets[i].num_members; j++ )
                                                printf("\t%s\n", members[j] );
                                }
			}
		}else if( Is_transition_mess(   service_type ) ) {
			printf("received TRANSITIONAL membership for group %s\n", sender );
		}else if( Is_caused_leave_mess( service_type ) ){
			printf("received membership message that left group %s\n", sender );
		}else printf("received incorrecty membership message of type 0x%x\n", service_type );
        } else if ( Is_reject_mess( service_type ) )
        {
		printf("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
			sender, service_type, mess_type, endian_mismatch, num_groups, ret, mess );
	}else printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);


	printf("\n");
	printf("User> ");
	fflush(stdout);
}

static	void	Usage(int argc, char *argv[])
{
	sprintf( User, "user" );
	sprintf( Spread_name, "4803");
	while( --argc > 0 )
	{
		argv++;

		if( !strncmp( *argv, "-u", 2 ) )
		{
                        if (argc < 2) Print_help();
                        strcpy( User, argv[1] );
                        argc--; argv++;
		}else if( !strncmp( *argv, "-r", 2 ) )
		{
			strcpy( User, "" );
		}else if( !strncmp( *argv, "-s", 2 ) ){
                        if (argc < 2) Print_help();
			strcpy( Spread_name, argv[1] ); 
			argc--; argv++;
		}else{
                    Print_help();
                }
	 }
}
static  void    Print_help()
{
    printf( "Usage: spuser\n%s\n%s\n%s\n",
            "\t[-u <user name>]  : unique (in this machine) user name",
            "\t[-s <address>]    : either port or port@machine",
            "\t[-r ]    : use random user name");
    exit( 0 );
}
static  void	Bye()
{
	To_exit = 1;

	printf("\nBye.\n");

	SP_disconnect( Mbox );

	exit( 0 );
}
