/*----------------------------------------------------------------------*
 *  								*
 *  COPYRIGHT(C)1997-2000 IIZUKA,Shinji. All Right reserved.		*
 *									*
 *  wbpop - WEB based POP3 client					*
 *									*
 *  author : IIZUKA,Shinji(siizuka@nurs.or.jp)				*
 *									*
 *----------------------------------------------------------------------*
 *									*
 *  parmeters(all parameters are retrieved by POST method)		*
 *									*
 *     s : pop3 server name						*
 *     u : user name							*
 *     p : password of user						*
 *     m : method							*
 *     c : command of method						*
 *     a : use apop when TRUE(1)					*
 *     t : port no(110 when blank)					*
 *        m = 0/none  initial panel					*
 *        m = 1       console mode view					*
 *        m = 2       retrieve subject ( c = no of msg: all when zero )	*
 *        m = 3       retrieve message ( c = no of msg: all when zero )	*
 *        m = 4       delete   subject ( c = no of msg: all when zero )	*
 *									*
 *----------------------------------------------------------------------*
 * history:								*
 *	1997.08.01	v1	created					*
 *	1997.10.02	v2	add MIME decode process			*
 *	1997.03.21	v3	Integrate wbmail.cgi			*
 *	1998.03.23	v3.1	Change "new-line" to server		*
 *	1998.03.25	v3.2	Decode user name and password		*
 *	1998.04.20	v4	Update method to retrieve/delete mail	*
 *	          		Add dump mode(contents of mailbox only)	*
 *	1998.05.27	v4.1	Add "-DLINUX" option for linux		*
 *	1998.08.12	v4.2	Display size of mail in Header mode	*
 *	1998.09.30	v4.3	enable to read the line starting "."	*
 *	1998.10.02	v4.4	Buf fix for v4.3			*
 *	1998.10.02	v4.5	Enable user information definition	*
 *	1999.01.26	v4.51	Bug fix for MIME-B Decode		*
 *	2000.03.16	v4.6	APOP authorization			*
 *				Enable other than 110 socket no.	*
 *	2002.04.07	v4.63	set alarm for troubled socket		*
 *----------------------------------------------------------------------*/
char	*version   = "Version 4.63(2002.04.07)";
char	*copyright = "Copyright(C)1997-2002 IIZUKA,Shinji(<A HREF=\"mailto:siizuka@nurs.or.jp\">siizuka@nurs.or.jp</A>). All right reserved.";
char	*ident = "@(#)wbpop.c 2002.04.07 V4.63";

/* Include header files	*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <stdarg.h>
#include "md5.h"

#ifdef	LINUX
#	include <sys/time.h>
#else
#	include <time.h>
#endif

#define	WAITING	30

/*----------------------------------------------------------------------*
 *									*
 *	Definition of macro constants / functions 			*
 *									*
 *----------------------------------------------------------------------*/

/* ------------------------------------ *
 * 	Logical type			*
 * ------------------------------------ */
#define	Boolean	int
#define	FALSE	0
#define	TRUE	!(FALSE)


/* ------------------------------------ *
 * 	MIME header			*
 * ------------------------------------ */
#define	HEADER_CONTENT_TYPE		"Content-Type: "
#define	HEADER_CONTENT_DISPOSITION	"Content-Disposition: "
#define	HEADER_SUBJECT			"Subject: "
#define	HEADER_FROM			"From: "
#define	HEADER_DATE			"Date: "

/* ------------------------------------ *
 * 	MIME encoding			*
 * ------------------------------------ */
#define	BE_HEAD	"=?ISO-2022-JP?B?"	/* Header of MIME B encode	*/
#define	QE_HEAD	"=?ISO-2022-JP?Q?"	/* Header of MIME Q encode	*/
#define	L_E_HEAD 16			/* Length of BE_HEAD,QE_HEAD */

#define	is_B_header(a)  !(strncmp(ucase((a),L_E_HEAD),BE_HEAD,L_E_HEAD))
#define	is_Q_header(a)  !(strncmp(ucase((a),L_E_HEAD),QE_HEAD,L_E_HEAD))
#define	is_mime_header(a)  ((is_B_header((a)))||(is_Q_header((a))))

/* for decoding MIME encoded string		*/
#define decode_mime(a,b,c,d)  (is_B_header((a))?decode_mime_B((a),(b),(c),(d)):decode_mime_Q((a),(b),(c),(d)))

/* decide in/out of JIS escaped characters	*/
#define	is_jis_in(a)	(!(strncmp((a),jis_in,3)))
#define	is_jis_out(a)	(!(strncmp((a),jis_out1,3))||!(strncmp((a),jis_out2,3)))

/* ------------------------------------ *
 * 	Method of CGI			*
 * ------------------------------------ */
#define	F_METHOD	"m"
#define	F_HOST		"s"
#define	F_USER		"u"
#define	F_PASSWD	"p"
#define	F_APOP		"a"
#define	F_PORTNO	"t"

#define	FM_CONSOLE	'1'
#define	FM_HEAD		'2'
#define	FM_PROPER	'3'
#define	FM_DUMP		'5'
#define	FM_INIT		(!(FM_CONSOLE)&&!(FM_HEAD)&&!(FM_PROPER)&&!(FM_DUMP))

/* ------------------------------------ *
 * 	other definition of constant	*
 * ------------------------------------ */
#define	HUGE	4096	/* bytes per line (long)	*/
#define	LINESIZ	256	/* bytes per line (command)	*/
#define	PTR_FOR_NULL	(void *)"\0"			/* pointer for Null	*/
#define is_eom(str)  (str[0]=='.'&&str[1]!='.')		/* end of mail		*/

/* ------------------------------------ *
 * 	other definition of constant	*
 * ------------------------------------ */
#define	memalloc(x,c)	(x *)malloc((sizeof(x))*c)	/* allocate (length of x)*c bytes	*/
#define	open_mon()	stdout				/* open output area	*/

/*----------------------------------------------------------------------*
 *									*
 *	Global variables						*
 *									*
 *----------------------------------------------------------------------*/
FILE	*fp_mon;		/* output file [ = stdout ]		*/
char	*pgmname;		/* name of this program [cmdline level]	*/
char	*cginame;		/* name of this program [cgi level]	*/
Boolean	is_dump = FALSE;	/* Use dump mode if TRUE		*/

char	s_line[LINESIZ];	/* command line to send to POP3 server	*/
char	r_line[HUGE];		/* response line from POP3 server	*/

unsigned	*mail_size;	/* size[octet] of mail			*/
unsigned	len_mail_size;


int	fd;			/* Socket */

/* socket interface (read UNIX manual if unable to understand) */
static struct sockaddr_in sin = { AF_INET };


/* MIME B encoding table. i.e:  t64['A'] == 0	*/
char	t64[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* 0 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* 1 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 	/* 2 */
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1, 	/* 3 */
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 	/* 4 */
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 	/* 5 */
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 	/* 6 */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, 	/* 7 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* 8 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* 9 */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* A */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* B */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* C */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* D */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 	/* E */
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};	/* F */
      /* 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F	*/


char	jis_in[3]   = { 0x1b, 0x24, 0x42 };	/* shift-in to ISO-2022-JP	*/
char	jis_out1[3] = { 0x1b, 0x28, 0x42 };	/* shift-out from ISO-2022-JP	*/
char	jis_out2[3] = { 0x1b, 0x28, 0x4A };

char	*getenv();

/************************************************************************
 *	base libraries							*
 ************************************************************************/


/*----------------------------------------------------------------------*
 *									*
 *	on_alarm() - exit itself	 				*
 *									*
 *----------------------------------------------------------------------*/
int	on_alarm()
{
	exit(0);
}

/*----------------------------------------------------------------------*
 *									*
 *	mon_print() - print into blowser				*
 *									*
 *----------------------------------------------------------------------*/
int	mon_print ( Boolean flag, char *format, ...)
{
	va_list arg;
	int	nbyte;

	if ( flag == TRUE ) {
		va_start(arg, format);
		nbyte = vfprintf(fp_mon, format, arg);
		va_end(arg);
		return (nbyte);
	} else {
		return ( 0 );
	}
}
/*----------------------------------------------------------------------*
 *									*
 *	mon_flush() - flush buffer into blowser				*
 *									*
 *----------------------------------------------------------------------*/
void	mon_flush ( Boolean flag )
{
	if ( flag == TRUE ) {
		fflush( fp_mon );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	hex2int()	- get number from hex-expressed charcter	*
 *									*
 *----------------------------------------------------------------------*/
int	hex2int( int c )
{
	switch ( c & 0xff ) {
	case '0' : 		return ( 0 );
	case '1' : 		return ( 1 );
	case '2' : 		return ( 2 );
	case '3' : 		return ( 3 );
	case '4' : 		return ( 4 );
	case '5' : 		return ( 5 );
	case '6' : 		return ( 6 );
	case '7' : 		return ( 7 );
	case '8' : 		return ( 8 );
	case '9' : 		return ( 9 );
	case 'a' : case 'A': 	return ( 10 );
	case 'b' : case 'B': 	return ( 11 );
	case 'c' : case 'C': 	return ( 12 );
	case 'd' : case 'D': 	return ( 13 );
	case 'e' : case 'E': 	return ( 14 );
	case 'f' : case 'F': 	return ( 15 );
	default : 		return( -1 );
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	str2int()	- convert string to integer(zero if not number)	*
 *									*
 *----------------------------------------------------------------------*/
int	str2int( char *str )
{
	int	ix;

	for ( ix = 0; ix < strlen(str); ix++ ) {
		if ( !(isdigit(str[ix])) ) { 
			return( 0 );
		}
	}
	return( atoi( str ) );
}


/*----------------------------------------------------------------------*
 *									*
 *	ucase()		- get Upper case of string			*
 *									*
 *----------------------------------------------------------------------*/
char	*ucase( char *s, int l )
{
	int	len, ix;
	static char	buf[HUGE];

	len = ( l <= 0 ? strlen(s) : l );
	for ( ix = 0; ix < len; ix++) {
		buf[ix] = (('a' <= s[ix] && s[ix] <= 'z') ? s[ix] - 0x20 : s[ix]) & 0xff;
	}
	buf[ix] = '\0';
	return ( buf );
}


/*----------------------------------------------------------------------*
 *									*
 *	get_date_str() - get string format of date			*
 *									*
 *----------------------------------------------------------------------*/

char	*get_date_str ()
{
	static char	ymd_hms[18+1];	/* return values	*/
	struct tm *ptr;		/* pointer for date	*/
	struct tm *localtime();
	time_t time(), nseconds;

	nseconds = time (NULL);
	ptr = localtime (&nseconds);

	sprintf(ymd_hms, "%d.%d.%d %d:%d:%d",
	    ptr->tm_year + 1900, ptr->tm_mon + 1, ptr->tm_mday,
	    ptr->tm_hour, ptr->tm_min, 	 ptr->tm_sec);
	return (ymd_hms);
}


/************************************************************************
 *	network libraries						*
 ************************************************************************/

/*----------------------------------------------------------------------*
 *									*
 *	connect_to_remote_host ( ) - create connecton			*
 *									*
 *----------------------------------------------------------------------*/
int	connect_to_remote_host( char *host, int portno )
{
	int	rc;
/*	int	s; */

	struct hostent *hp;
	struct servent *sp;

	sin.sin_family = AF_INET;

	mon_print (!(is_dump), "<FONT COLOR=green>resolving %s. ", host );
	mon_print (!(is_dump), "please wait...<BR></FONT>\n" );
	mon_flush(!(is_dump));

	sp = malloc ( sizeof( struct servent ) );
	sp->s_port = htons(portno);

	if ( ( hp = gethostbyname( host ) ) == 0 ) {
		mon_print( !(is_dump), "<FONT COLOR=red>cannot resolve POP" );
		mon_print( !(is_dump), " server %s</FONT><BR>\n", host );
		mon_print( !(is_dump), "<FONT COLOR=red>It may be due to too " );
		mon_print( !(is_dump), "many connections or server name " );
		mon_print( !(is_dump), "mismatch.</FONT><BR>\n" );
		mon_flush( !(is_dump) );
		return ( -1 );
	}
	sin.sin_port = sp->s_port;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		mon_print( !(is_dump), "<FONT COLOR=red>fatal: failed to call socket() (%s)</FONT><BR>\n", strerror( errno ) );
		mon_flush( !(is_dump) );
		return ( fd );
	}

	sin.sin_family = hp->h_addrtype;
/*
	bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
*/
	memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	sin.sin_port = sp->s_port;

	mon_print( !(is_dump), "<FONT COLOR=green>connecting %s using port no. %d...<BR></FONT>\n", host,portno );

	alarm( WAITING );
	rc = connect(fd, (struct sockaddr *) & sin, sizeof(sin));
	alarm( 0 );

	if ( rc < 0) {
		mon_print( !(is_dump), "<FONT COLOR=red>failed to call connect() (%s)</FONT><BR>\n",strerror( errno ) );
		mon_flush( !(is_dump) );
		mon_flush( !(is_dump) );
		mon_print( !(is_dump),"<FONT COLOR=red>It may be due to too many connections or server down.</FONT><BR>\n" );
		mon_flush( !(is_dump) );
		shutdown( fd, 2 );
		close ( fd );
		return ( rc );
	} else {
		return ( fd );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	poll_fd() - poll file 						*
 *									*
 *----------------------------------------------------------------------*/
int	poll_fd ( int fd, char *mode, int poll_time )
{
	fd_set	fds;
	int	nfound;
	struct timeval timeout;

	FD_ZERO ( &fds );
	FD_SET (fd, &fds);
	timeout.tv_sec  = poll_time;
	timeout.tv_usec = 0;

	if ( *mode == 'r' ) {
		nfound = select(fd + 1, &fds, 0, 0, &timeout);
	} else if ( *mode == 'w' ) {
		nfound = select(fd + 1, 0, &fds, 0, &timeout);
	}

	if ( nfound < 0 ) {
		return ( -1 );
	} else if ( nfound == 0 ) {
		return ( 0 );
	} else {
		if ( FD_ISSET ( fd, &fds ) ) {
			return ( 1 );
		} else {
			return ( 0 );
		}
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	readline() - read one line from fd 				*
 *									*
 *----------------------------------------------------------------------*/
int	readline ( int fd, char *buf )
{
	int	ix;
	char	c, nbyte;

	ix = 0;
	while (1) {
		alarm( WAITING );
		nbyte = read ( fd, &c, 1 );
		alarm( 0 );
		if ( nbyte <= 0 ) {
			buf[ix] = '\0';
			return ( nbyte < 0 ? nbyte : ix ) ;
		}
		if ( c == '\n' ) {
			if ( ix > 0 && buf[ix - 1] == '\r' ) {
				buf[ix++] = c;
				buf[ix]   = '\0';
				return ( ix );
			} else {
				/* No Statement	*/;
			}
		} else {
			buf[ix++] = c;
		}
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	write_to_sock() - write to remote connection			*
 *									*
 *----------------------------------------------------------------------*/
int	write_to_sock ( int fd, char *line )
{
	int	len, nbyte;

	len = strlen ( line );
	alarm( WAITING );
	nbyte = write ( fd, line, len );
	alarm( 0 );
	if ( nbyte != len ) {
		mon_print( !(is_dump), "<FONT COLOR=red>fatal: system error occured to call write() (%s).</FONT>", strerror( errno ) );
		mon_flush( !(is_dump) );
		return ( -1 );
	} else {
		return ( nbyte );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	print_info() - Display Information area				*
 *									*
 *----------------------------------------------------------------------*/
void	print_info ( char *infofile )
{
	FILE	 * fp;
	char	buf[HUGE];

	if ( ( fp = fopen ( infofile, "r" ) ) != NULL ) {
		while ( ( fgets ( buf, HUGE, fp ) ) != NULL ) {
			fputs ( buf, fp_mon );
			mon_flush(!(is_dump));
		}
		fclose ( fp );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	signal handler							*
 *									*
 *----------------------------------------------------------------------*/
void	on_end()
{
	if ( !is_dump ) {
#ifdef	USER_INFO_2
		print_info( USER_INFO_2 );
#endif
		mon_print( !(is_dump), "</BODY></HTML>" );
	}
}


void	on_intr()
{
	mon_print( !(is_dump),"<FONT COLOR=red>Interrupt caught.process terminated.<BR></FONT>\n" );
	mon_flush( TRUE );
	exit ( 0 );
}


/************************************************************************
 *	CGI libraries							*
 ************************************************************************/

/*----------------------------------------------------------------------*
 *									*
 *	get_cginame() - just as name					*
 *									*
 *----------------------------------------------------------------------*/

char	*get_cginame ( char *dest )
{
	char	*p;
	int	ix, max_ix, ix_set;

	for ( ix = strlen( dest ) - 1; ix >= 0 && dest[ix] != '/'; ix--)
		;

	p = memalloc( char, strlen( dest ) - ix );

	ix_set = 0;
	for ( ix++; dest[ix] != '\0'; ix++) {
		p[ix_set++] = dest[ix];
	}

	p[ix_set] = '\0';
	return ( p );
}


/*----------------------------------------------------------------------*
 *									*
 *	url_decode()	- decode URL-encoded Data			*
 *									*
 *----------------------------------------------------------------------*/

char	*url_decode ( char *in )
{
	int	ix, ix_set;		/* as you assume		*/
	static char	out[LINESIZ];	/* area for return value	*/

	int	len; 
	len = strlen(in);

	for ( ix = ix_set = 0; ix < len; ix++) {
		if ( in[ix] == '+' ) {
			out[ix_set++] = ' ';
		} else if ( in[ix] == '%' && ix < len - 2 ) {
			out[ix_set++] = 0xff & (hex2int(in[ix+1]) * 0x10 + hex2int(in[ix+2]));
			ix += 2;
		} else {
			out[ix_set++] = in[ix];
		}
	}
	out[ix_set] = '\0';
	return ( out );
}


/*----------------------------------------------------------------------*
 *									*
 *	query_parm() - query cgi parameter				*
 *									*
 *----------------------------------------------------------------------*/
char	*query_parm( char *key )
{
	static char	*query_key[256], *query_val[256];
	int	content_length;
	char	*envp, *query_string;
	int	ix;

	if ( key == NULL ) {
		/* decide whether method to retrieve parameters */
		envp = getenv ( "REQUEST_METHOD" );
		if ( envp == NULL || *envp == '\0' ) {
			query_key[0] = NULL;
			return ( NULL );
		}
		/* set parameters into query_key[]	*/
		if ( !(strncmp ( envp, "GET", 3 ) ) ) {
			envp = getenv( "QUERY_STRING" ) ;
			if ( envp == NULL || *envp == '\0' ) {
				query_key[0] = ( char *) malloc( sizeof (char *) );
				query_key[0] = NULL;
				return ( NULL );
			} else {
				query_string = 
				    malloc( strlen(getenv("QUERY_STRING")) + 1 );
				strcpy ( query_string, getenv("QUERY_STRING"));
			}
		} else {
			envp = getenv( "CONTENT_LENGTH" );
			content_length = atoi( envp );
			query_string = malloc( content_length + 1 );
			read ( 0, query_string, content_length );
			query_string[content_length] = '\0';
		}
		/* pharse into array	*/
		for ( ix = 0; 1 ; ix ++) {
			query_key[ix] = strtok(ix == 0 ? query_string : NULL, "=" );
			if ( query_key[ix] == NULL ) {
				query_val[ix] = NULL;
				break;
			}
			query_val[ix] = strtok(NULL, "&" );
			if ( query_val[ix] == NULL ) {
				break;
			}
		}
		return ( NULL ) ;
	} else {
		for ( ix = 0 ; query_key[ix] != NULL; ix++) {
			if ( strlen( query_key[ix] ) == strlen( key ) ) {
				if ( !strcmp ( query_key[ix], key ) ) {
					return ( query_val[ix] );
				}
			}
		}
		return ( NULL ) ;
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	real_parm() - query "real" parm for CGI				*
 *	(returns _pointer for_ Null when parm == NULL)			*
 *									*
 *----------------------------------------------------------------------*/
char	*real_parm( char *key )
{
	char	*p;
	p = query_parm( key );
	return ( p == NULL ? PTR_FOR_NULL : p );
}


/************************************************************************
 *	functions							*
 ************************************************************************/

/*----------------------------------------------------------------------*
 *									*
 *	input_cmd() - send cmd to POP server until max retry(2)		*
 *		reached or execution of command successed		*
 *									*
 *----------------------------------------------------------------------*/
int	input_cmd ( int fd, char *cmd, char *res )
{
	int	ix;			/* You already know what it is	*/
	char	cmd_with_crnl[LINESIZ];	/* cmd + \r + \n		*/

	sprintf ( cmd_with_crnl, "%s\r\n", cmd );

	for ( ix = 0; ix < 3; ix++) {
		mon_print( !(is_dump),"<FONT COLOR=blue>%s<BR></FONT>\n", cmd );
		mon_flush(!(is_dump));
		if ( write_to_sock ( fd, cmd_with_crnl ) < 0 ) {
			return ( 0 );
		}
		readline ( fd, res );
		mon_print( !(is_dump), "%s<BR>\n", res );
		if ( res[0] != '-' ) {
			return ( 1 );
		}
	}
	return ( 0 );
}


/*----------------------------------------------------------------------*
 *									*
 *	input_cmd_crypt() - send cmd to POP server until max retry(2)	*
 *		reached or execution of command successed		*
 *									*
 *----------------------------------------------------------------------*/
int	input_cmd_crypt ( int fd, char *cmd, char *res )
{
	int	ix;
	static char	c_buf[LINESIZ];
	char	cmd_with_crnl[LINESIZ];	/* cmd + \r + \n		*/

	strcpy ( c_buf, cmd );
	sprintf ( cmd_with_crnl, "%s\r\n", cmd );

	if ( !is_dump ) {
		for ( ix = 4; ix < strlen( c_buf ); ix++) {
			switch ( c_buf[ix] ) {
			case ' ': 
			case '\r': 
			case '\n'	: 
				break;
			default : 
				c_buf[ix] = '*';
			}
		}
	}

	for ( ix = 0; ix < 3; ix++) {
		mon_print( !(is_dump), "<FONT COLOR=blue>%s<BR></FONT>\n", c_buf );
		mon_flush(!(is_dump));
		if ( write_to_sock ( fd, cmd_with_crnl ) < 0 ) {
			return ( 0 );
		}
		readline ( fd, res );

		mon_print( !(is_dump),"%s<BR>\n", res );
		if ( res[0] != '-' ) {
			return ( 1 );
		}
	}
	return ( 0 );
}


/*----------------------------------------------------------------------*
 *									*
 *	retr_mail_list() - retrieve number of mail(s)			*
 *									*
 *----------------------------------------------------------------------*/
int	retr_mail_list(int fd)
{
	int	mail_cnt;
	int	is_allocatable;

	if ( !( input_cmd ( fd, "LIST", r_line ) ) ) {
		alarm( WAITING );
		shutdown( fd, 2 );
		close ( fd );
		alarm( 0 );
		exit ( 0 );
	}

	len_mail_size = 0;
	if ( ( mail_size = ( unsigned *) malloc( sizeof(unsigned) ) ) == NULL ) {
		is_allocatable = FALSE;
	} else {
		is_allocatable = TRUE;
	}

	mail_cnt = 0;
	while ( readline ( fd, r_line ) > 0 ) {
		mon_print( !(is_dump),"%s<BR>\n",  r_line );
		mon_flush( !(is_dump) );

		if ( r_line[0] == '.' ) {
			break;
		} else {
			mail_cnt++;
			mail_cnt = atoi ( strtok( r_line, " " ));
			if ( is_allocatable ) {
				if ( (mail_size = 
				        (unsigned *)realloc(mail_size, mail_cnt * sizeof(unsigned *))
				      ) != NULL ) {
					mail_size[mail_cnt - 1] = atoi( strtok( NULL, "\r\n " ));
					len_mail_size++;
				} else {
					is_allocatable = FALSE;
				}
			}
		}
	}

	return ( mail_cnt );
}


/*----------------------------------------------------------------------*
 *									*
 *	is_mail_to_process() - get whether this mail is to process	*
 *									*
 *----------------------------------------------------------------------*/
Boolean	is_mail_to_process(int no, char key)
{
	char	sk[256];

	sprintf ( sk, "%c%d", key, no );
	if ( (query_parm(sk) != NULL) && !(strcmp(ucase(query_parm(sk), 0), "ON")) ) {
		return ( TRUE ) ;
	} else {
		return ( FALSE );
	}
}




/*----------------------------------------------------------------------*
 *									*
 *	display_input_area() - display input area of server,user,passwd	*
 *									*
 *----------------------------------------------------------------------*/

void	display_input_area ( /* No Argument */ )
{
	mon_print( TRUE, "<TABLE BORDER=1>\n" );

	mon_print( TRUE, "<TR>\n" );
	mon_print( TRUE, "<TD>DNS name of your POP3 server</TD>\n" );
	mon_print( TRUE, "<TD><INPUT TYPE=text NAME=%s SIZE=60 value=%s></TD>\n", F_HOST, real_parm(F_HOST) );
	mon_print( TRUE, "</TR>\n" );

	mon_print( TRUE, "<TR>\n" );
	mon_print( TRUE, "<TD>Port number of your POP3 server<BR>(110 when blank)</TD>\n" );
	mon_print( TRUE, "<TD><INPUT TYPE=text NAME=%s SIZE=10 VALUE=%s></TD>\n",
	    F_PORTNO, (*real_parm(F_PORTNO)) != '\0' ? real_parm(F_PORTNO) : "110" );
	mon_print( TRUE, "</TR>\n" );

	mon_print( TRUE, "<TR>\n" );
	mon_print( TRUE, "<TD>Your account</TD>\n" );
	mon_print( TRUE, "<TD><INPUT TYPE=text NAME=%s SIZE=60 value=%s></TD>\n", F_USER, real_parm(F_USER) );
	mon_print( TRUE, "</TR>\n" );

	mon_print( TRUE, "<TR>\n" );
	mon_print( TRUE, "<TD>Password for your account</TD>\n" );
	mon_print( TRUE, "<TD><INPUT TYPE=password NAME=%s SIZE=60></TD>\n", F_PASSWD );
	mon_print( TRUE, "</TR>\n" );

	mon_print( TRUE, "</TABLE>\n" );

	mon_print( TRUE, "<P><INPUT TYPE=radio NAME=%s VALUE=%c>Console mode\n", F_METHOD, FM_CONSOLE );

	mon_print( TRUE, "<INPUT TYPE=radio NAME=%s VALUE=%c %s>Header only\n",
	    F_METHOD, FM_HEAD, (*real_parm(F_METHOD)) != FM_HEAD ? "CHECKED" : PTR_FOR_NULL );

	mon_print( TRUE, "<INPUT TYPE=radio NAME=%s VALUE=%c %s>Retrieve/Delete Seleced Mail\n",
	    F_METHOD, FM_PROPER, (*real_parm(F_METHOD)) == FM_HEAD ? "CHECKED" : PTR_FOR_NULL );

	mon_print( TRUE, "<INPUT TYPE=radio NAME=%s VALUE=%c>Dump Mode\n", F_METHOD, FM_DUMP );

	mon_print( TRUE, "<BR><INPUT TYPE=checkbox NAME=%s>Use APOP authorization<BR>\n", F_APOP );

	mon_print( TRUE, "<SCRIPT LANGUAGE=\"JavaScript\">\n" );
	mon_print( TRUE, "<!--//\n" );
	mon_print( TRUE, "document.write ( \'<INPUT TYPE=button value=\"Execute\" onClick=\"execute_it();return(0);\">\' );\n" );
	mon_print( TRUE, "//-->\n" );
	mon_print( TRUE, "</SCRIPT>\n" );
	mon_print( TRUE, "<NOSCRIPT>\n" );
	mon_print( TRUE, "<INPUT TYPE=submit value=\"Execute\">\n" );
	mon_print( TRUE, "</NOSCRIPT>\n" );

	mon_print( TRUE, "<INPUT TYPE=reset value=\"Reset\">\n" );

	mon_print( TRUE, "</P>\n" );
	mon_flush(!(is_dump));
}

/*----------------------------------------------------------------------*
 *									*
 *	print_js_head() - print header line of javascript		*
 *									*
 *----------------------------------------------------------------------*/
void	print_js_head()
{
	mon_print( TRUE, "<SCRIPT LANGUAGE=\"JavaScript\">\n");
	mon_print( TRUE, "<!--//\n");
	mon_print( TRUE, "read_key = \"R\"; delete_key = \"D\";\n");
	mon_print( TRUE, "function execute_it( ) {\n");
	mon_print( TRUE, "  for ( ix = 0; ix < document.f.m.length && document.f.m[ix].value != 3; ix++ ) ;\n");
	mon_print( TRUE, "  if ( document.f.m[ix].value == 3 ) {\n");
	mon_print( TRUE, "    if ( document.f.m[ix].checked ) {\n");
	mon_print( TRUE, "      check_cnt = 0\n");
	mon_print( TRUE, "      for ( ix = 0; ix < document.f.elements.length; ix++ ) {\n");
	mon_print( TRUE, "        if ( document.f.elements[ix].type == \"checkbox\" ) {\n");
	mon_print( TRUE, "          ident = document.f.elements[ix].name.substring(0,1);\n");
	mon_print( TRUE, "          if ( ident == read_key || ident == delete_key ) {\n");
	mon_print( TRUE, "            if ( document.f.elements[ix].checked == true ) {\n");
	mon_print( TRUE, "              check_cnt++;\n");
	mon_print( TRUE, "            }\n");
	mon_print( TRUE, "          }\n");
	mon_print( TRUE, "        }\n");
	mon_print( TRUE, "      }\n");
	mon_print( TRUE, "      if ( check_cnt < 1 ) {\n");
	mon_print( TRUE, "        alert( \"Check \\\"Read\\\" checkbox or \\\"Delete\\\" checkbox.\" );\n");
	mon_print( TRUE, "        return(0);\n");
	mon_print( TRUE, "      }\n");
	mon_print( TRUE, "    }\n");
	mon_print( TRUE, "  }\n");

	mon_print( TRUE, "  if ( document.f.s.value == \"\" ) {\n");
	mon_print( TRUE, "    alert( \"Input the name of your POP3 Server.\" );\n");
	mon_print( TRUE, "    document.f.s.focus();\n");
	mon_print( TRUE, "    return(0);\n");
	mon_print( TRUE, "  }\n");
	mon_print( TRUE, "  if ( document.f.u.value == \"\" ) {\n");
	mon_print( TRUE, "    alert( \"Input your account.\" );\n");
	mon_print( TRUE, "    document.f.u.focus();\n");
	mon_print( TRUE, "    return(0);\n");
	mon_print( TRUE, "  }\n");
	mon_print( TRUE, "  if ( document.f.p.value == \"\" ) {\n");
	mon_print( TRUE, "    alert( \"Input the password of your account.\" );\n");
	mon_print( TRUE, "    document.f.p.focus();\n");
	mon_print( TRUE, "    return(0);\n");
	mon_print( TRUE, "  }\n");
	mon_print( TRUE, "  document.f.submit();\n");
	mon_print( TRUE, "}\n");

	mon_print( TRUE, "//-->\n");
	mon_print( TRUE, "</SCRIPT>\n");
	mon_print( TRUE, "<BR>\n" );
}

/*----------------------------------------------------------------------*
 *									*
 *	print_js_select_all_mail() - as the name of function		*
 *									*
 *----------------------------------------------------------------------*/
void	print_js_select_all_mail()
{
	mon_print( TRUE, "<SCRIPT LANGUAGE=\"JavaScript\">\n");
	mon_print( TRUE, "<!--//\n");
	mon_print( TRUE, "function check_all( k,b ) {\n");
	mon_print( TRUE, "  for ( ix = 0; ix < document.f.elements.length; ix++ ) {\n");
	mon_print( TRUE, "    if ( document.f.elements[ix].type == \"checkbox\" ) {\n");
	mon_print( TRUE, "      if ( document.f.elements[ix].name.substring(0,1) == k ) { \n");
	mon_print( TRUE, "        if ( document.f.elements[ix].checked != b ) { \n");
	mon_print( TRUE, "          document.f.elements[ix].click();\n");
	mon_print( TRUE, "        }\n");
	mon_print( TRUE, "      }\n");
	mon_print( TRUE, "    }\n");
	mon_print( TRUE, "  }\n");
	mon_print( TRUE, "}\n");

	mon_print( TRUE, "//-->\n");
	mon_print( TRUE, "</SCRIPT>\n");
}


/*----------------------------------------------------------------------*
 *									*
 *	print_javascript_indicate() - as the name of function		*
 *									*
 *----------------------------------------------------------------------*/
void	print_javascript_indicate()
{
	mon_print( TRUE, "<SCRIPT LANGUAGE=\"JavaScript\">\n");
	mon_print( TRUE, "<!--//\n");

	mon_print( TRUE, "document.write ( '<INPUT TYPE=BUTTON VALUE=\"ON all(read)\" onClick=\"check_all(read_key,true);return(0)\">');\n");
	mon_print( TRUE, "document.write ( '<INPUT TYPE=BUTTON VALUE=\"OFF all(read)\" onClick=\"check_all(read_key,false);return(0)\"> ');\n");
	mon_print( TRUE, "document.write ( '<INPUT TYPE=BUTTON VALUE=\"ON all(del)\" onClick=\"check_all(delete_key,true);return(0)\">');\n");
	mon_print( TRUE, "document.write ( '<INPUT TYPE=BUTTON VALUE=\"OFF all(del)\" onClick=\"check_all(delete_key,false);return(0)\">');\n");
	mon_print( TRUE, "//-->\n");
	mon_print( TRUE, "</SCRIPT><BR>\n");
}

/*----------------------------------------------------------------------*
 *									*
 *	display_initial_panel()	- just as name of function		*
 *									*
 *----------------------------------------------------------------------*/
int	display_initial_panel()
{
#ifdef	USR_NPH_MODE
	mon_print( TRUE, "HTTP/1.0 200 OK\n" );
#endif
	mon_print( TRUE, "Content-type: text/html\n\n" );
	mon_print( TRUE, "<HTML><HEAD><TITLE>Web Based POP3 Service" );
	mon_print( TRUE, "</TITLE></HEAD><BODY BGCOLOR=white><CENTER>" );
	mon_print( TRUE, "<FONT SIZE=+2>Web Based POP3 Service</FONT>" );
	mon_print( TRUE, "</CENTER><P>" );
	display_copyright( "RIGHT" );
	mon_print( TRUE, " <FORM NAME=f METHOD=POST ACTION=\"%s\"></P>\n", cginame );
	display_input_area ( ) ;
	mon_print( TRUE, "</FORM>\n" );
}


/*----------------------------------------------------------------------*
 *									*
 *	display_copyright()	- just as name of function		*
 *									*
 *----------------------------------------------------------------------*/
int	display_copyright(char *align)
{
	mon_print( TRUE, "<P ALIGIN=%s>", align );
	mon_print( TRUE, "%s.<BR>\n", version );
	mon_print( TRUE, "%s<BR>\n", copyright );
	mon_print( TRUE, " This page is served &quot;AS IS&quot;." );
	mon_print( TRUE, "Use this service at your own risk</P>\n" );
	mon_print( TRUE, "</FONT>\n" );
	print_js_head();

#ifdef	USER_INFO_1
	print_info( USER_INFO_1 );
#endif
}


/*----------------------------------------------------------------------*
 *									*
 *	is_url_char() - decide whether this charcter is a part of URL	*
 *									*
 *----------------------------------------------------------------------*/

Boolean is_url_char ( unsigned int c )
{
	if ( 0x21 <= (c & 0xff)  && (c & 0xff) <= 0x7e ) {
		switch ( c & 0xff ) {
		case '<' : 
		case '>' : 
		case '(' : 
		case ')' :
		case '[' : 
		case ']' : 
		case '{' : 
		case '}' :
		case ',' : 
		case '*' : 
		case '$' : 
		case '`' :
		case ';' : 
		case '^' : 
		case '\\': 
		case '\"':
		case '\'':
			return ( FALSE );
		default:
			return ( TRUE );
		}
	} else {
		return ( FALSE );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	decode_mime_Q ( ) - convert MIME Q encodeed data		*
 *									*
 *----------------------------------------------------------------------*/

int	decode_mime_Q ( unsigned char *in, unsigned char *out,
int *in_len, int *out_len )
{
	unsigned char	*p;

	int	ix, ix_out;

	/* determin start pointer of data area */
	p = in + strlen ( QE_HEAD );

	ix = 0; 
	ix_out = 0;
	while ( ix < strlen ( p ) && strncmp( p + ix, "?=", 2 ) != 0 ) {
		switch ( p[ix] ) {
		case '=':
			out[ix_out++] = 
			    0xff & (hex2int(p[ix+1]) * 16 + hex2int(p[ix+2]));
			ix += 3;
			break;
		case '_':
			out[ix_out++] = ' ';
			ix++;
			break;
		default:
			out[ix_out++] = p[ix++];
			break;
		}
	}
	out[ix_out] = '\0';

	*in_len  = ix + strlen( QE_HEAD ) + 2;
	*out_len = ix_out;
	return ( ix_out );
}


/*----------------------------------------------------------------------*
 *									*
 *	decode_mime_B ( ) - convert MIME B encodeed data		*
 *									*
 *----------------------------------------------------------------------*/

int	decode_mime_B ( unsigned char *in, unsigned char *out,
int *in_len, int *out_len )
{
	unsigned char	*p;
	char	cv[3];

	int	ix, ix_out, ix_cv;

	/* determin start pointer of data area */
	p = in + strlen ( BE_HEAD );

	ix = 0; 
	ix_out = 0;
	while ( ix < strlen ( p ) && strncmp( p + ix, "?=", 2 ) != 0 ) {
		/* decode first character */
		cv[0] = (( t64[p[ix]]   << 2 ) & 0xfc )
		 + ((t64[p[ix+1]] >> 4 ) & 0x03);
		if ( p[ix+2] == '=' ) {
			ix_cv = 1;
		} else {
			cv[1] = (( t64[p[ix+1]] << 4 ) & 0xf0 )
			 + ((t64[p[ix+2]] >> 2 ) & 0x0f);
			if ( p[ix+3] == '=' ) {
				ix_cv = 2;
			} else {
				ix_cv = 3;
				cv[2] = (( t64[p[ix+2]] << 6 ) & 0xC0 )
				 + ( t64[p[ix+3]] & 0x3f);
			}
		}
		memcpy ( out + ix_out, cv, ix_cv );
		ix += 4; 
		ix_out += ix_cv;
	}

	out[ix_out] = '\0';

	*in_len  = ix + strlen ( BE_HEAD ) + 2;
	*out_len = ix_out;

	return ( ix_out );
}


/*----------------------------------------------------------------------*
 *									*
 *	decode_str_of_mime () - convert mail into adequate format		*
 *									*
 *----------------------------------------------------------------------*/

int	decode_str_of_mime( char *in, long len, char *out )
{
	int	ix, ix_out, cv_in_len, decoded_char;
	unsigned char	*u_in, *u_out;

	u_in  = ( unsigned char *) in;
	u_out = ( unsigned char *) out;

	/* decode all MIME character */
	ix = 0, ix_out = 0;
	while ( ix < len - L_E_HEAD ) {
		if ( is_mime_header ( u_in + ix ) ) {
			decode_mime ( u_in + ix, u_out + ix_out,
			    &cv_in_len, &decoded_char );
			ix     += cv_in_len;
			ix_out += decoded_char;
		} else {
			u_out[ix_out] = u_in[ix];
			ix++; 
			ix_out++;
		}
	}

	memcpy ( (char *)u_out + ix_out, in + ix, len - ix + 1 );
	out[ix_out + len - ix + 1 ] = '\0';
}


/*----------------------------------------------------------------------*
 *									*
 *	strip_dummy_period () - strip dummy period			*
 *									*
 *----------------------------------------------------------------------*/

int	strip_dummy_period ( char *in )
{
	int	ix, len;

	len = strlen ( in );

	if ( in[0] == '.' && len > 3 ) {
		for ( ix = 0; ix < len - 1; ix++) {
			in[ix] = in[ix + 1];
		}
		ix--;
	} else {
		ix = len;
	}
	return ( ix );
}

/*----------------------------------------------------------------------*
 *									*
 *	get_apop_hex_didgest() - get hex expression of MD5 digest	*
 *				 for APOP authorization			*
 *									*
 *----------------------------------------------------------------------*/
void	get_apop_hex_didgest( char *line, char *password, char *apop_str )
{
	int	ix,ix_set;
	char	apop_salt[LINESIZ];
	unsigned char digest[LINESIZ];
	MD5_CTX	context;


	/* Find the start of timestamp */
	for (ix = 0    ; line[ix] != '\0' && line[ix] != '<'; ix++ );
	for (ix_set = 0; line[ix] != '\0' && line[ix] != '>'; ix++ ) {
		apop_salt[ix_set++] = line[ix];
	}
	if( ix_set <= 1 ) {	/* error if Null or '<' only */
		*apop_str = '\0';
		return;
	}

	apop_salt[ix_set++] = '>';
	apop_salt[ix_set]   = '\0';
	strcat(apop_salt, password );

	MD5Init(&context);
	MD5Update(&context, (unsigned char *)apop_salt, strlen(apop_salt));
	MD5Final(digest, &context);

	for (ix = 0;  ix < 16;  ix++) {	/* this ix is differnt from previous ix */
	       sprintf( apop_str + (ix * 2), "%02x", digest[ix]);
	}
	apop_str[ix * 2] = 0;
	return;
}

/*----------------------------------------------------------------------*
 *									*
 *	login_to_mail_server () - login					*
 *									*
 *----------------------------------------------------------------------*/
int	login_to_mail_server( int fd )
{
	int	nbyte;
	static char	apop_str[LINESIZ];

	while ( 1 ) {
		if ( poll_fd ( fd, "r", 3 ) <= 0 ) {
			break;
		}
		nbyte = readline ( fd, r_line );
		if ( nbyte <= 0 ) {
			break;
		}
	}
	mon_print( !(is_dump), "%s<BR>\n", r_line );
	mon_flush( !(is_dump) );

	*apop_str = '\0';
	if ( strcmp( ucase(real_parm(F_APOP),0), "ON" ) == 0 ) {
	/* APOP authorization */
		get_apop_hex_didgest( r_line, url_decode(real_parm(F_PASSWD)), apop_str );
		if ( *apop_str != '\0' ) {
			sprintf ( s_line, "APOP %s %s\r\n",url_decode(real_parm(F_USER)), apop_str );
			if ( !( input_cmd ( fd, s_line, r_line ) ) ) {
				alarm( WAITING );
				shutdown( fd, 2 );
				close ( fd );
				alarm( 0 );
				return ( -1 );
			}
			return ( 0 );
		}
	}
	if ( *apop_str == '\0' ) { /* when no APOP timestamp or old POP3 indicated */
	/* old POP3 authorization */
		/* Connect by user */
		sprintf ( s_line, "USER %s", url_decode(real_parm(F_USER)) );
		if ( !( input_cmd ( fd, s_line, r_line ) ) ) {
			alarm( WAITING );
			shutdown( fd, 2 );
			close ( fd );
			alarm( 0 );
			return ( -1 );
		}

		/* Ask password */
		sprintf ( s_line, "PASS %s", url_decode(real_parm(F_PASSWD)) );
		if ( !( input_cmd_crypt ( fd, s_line, r_line ) ) ) {
			alarm( WAITING );
			shutdown( fd, 2 );
			close ( fd );
			alarm( 0 );
			return ( -1 );
		}
		return ( 0 );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	set_anchor() - set <A> tag to line				*
 *									*
 *----------------------------------------------------------------------*/
int	set_anchor ( char *in, char *out, int *in_clen, int *out_clen )
{
	int	ixi, set_len, ixo, max_ixi;

	/* URL must be start with "http://" 	*/
	if ( strlen( in ) < 8 || strncmp(in, "http://", 7) != 0 || !(is_url_char(in[7])) ) {
		*in_clen = *out_clen = 0;
		return ( 0 );
	}

	/* set <A> tag	*/
	memcpy ( out, "<A HREF=\"", 9 );
	for ( ixi = 0, ixo = 9; is_url_char(in[ixi]); ixi++, ixo++) {
		out[ixo] = in[ixi];
	}
	set_len = ixi;
	memcpy ( out + ixo, "\">", 2 );
	ixo += 2;

	memcpy ( out + ixo, in, set_len );	/* set content	*/
	ixo += set_len;

	memcpy ( out + ixo, "</A>", 4 );	/* set </A> tag	*/
	ixo += 4;

	/* set length	*/
	*in_clen = ixi;
	*out_clen = ixo;
}


/*----------------------------------------------------------------------*
 *									*
 *	fmt_into_txt() -	format input line into text		*
 *									*
 *----------------------------------------------------------------------*/
int	fmt_into_txt( unsigned char *r_line, int nbyte, unsigned char *buf )
{
	static Boolean	is_shift_in = FALSE;
	int	ix, ix_buf, len_from, len_to;

	for ( ix = ix_buf = 0; ix < nbyte; ix++) {
		/* check shift mode	*/
		if ( ix <= nbyte - 3 ) {
			if ( is_jis_in ( r_line + ix ) ) {
				is_shift_in = TRUE;
			} else if ( is_jis_out ( r_line + ix ) ) {
				is_shift_in = FALSE;
			}
		}
		/* when this char is non-sjis code */
		if ( is_shift_in == FALSE ) {
			switch ( r_line[ix] ) {
			case '<':
				memcpy ( &(buf[ix_buf]), "&lt;", 4 );
				ix_buf += 4;
				break;
			case '>':
				memcpy ( &(buf[ix_buf]), "&gt;", 4 );
				ix_buf += 4;
				break;
			case '\"':
				memcpy ( &(buf[ix_buf]), "&quot;", 6 );
				ix_buf += 6;
				break;
			case 'h':
				set_anchor( r_line + ix, buf + ix_buf, &len_from, &len_to );
				if ( len_from <= 0 ) {
					buf[ix_buf] = r_line[ix];
					ix_buf ++;
				} else {
					ix_buf += len_to;
					ix     += len_from - 1;
				}
				break;
			default:
				buf[ix_buf] = r_line[ix];
				ix_buf ++;
				break;
			}
		} else {
			buf[ix_buf] = r_line[ix];
			ix_buf++;
		}
	}
	buf[ix_buf] = '\0';
	return ( 1 );
}


/*----------------------------------------------------------------------*
 *									*
 *	get_mail_date() - get date of mail				*
 *									*
 *----------------------------------------------------------------------*/
char	*get_mail_date ( char *line, char *date )
{
	int	len;
	len = strlen( line );

	switch ( line[6] ) {
	case '0': 
	case '1': 
	case '2': 
	case '3': 
	case '4':
	case '5': 
	case '6': 
	case '7': 
	case '8': 
	case '9':
		len = ( len > 26 ? 20: len - 6 );
		if ( len > 0 ) {
			memcpy ( date, &(line[6]), len ) ;
			date[len] = '\0';
		} else {
			date[0] = '\0';
		}
		return( date );
	default:
		len = ( len > 31 ? 20: len - 11 );
		if ( len > 0 ) {
			memcpy ( date, &(line[11]), len ); 
			date[len] = '\0';
		} else {
			date[0] = '\0';
		}
		return( date );
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	retrieve_header_line() - retrieve a line of header		*
 *									*
 *----------------------------------------------------------------------*/
int	retrieve_header_line( int fd, char *buf, Boolean is_first_indicate )
{
	static	char	prev_rec[HUGE];
	static  Boolean is_first = TRUE;
	static  Boolean is_last  = FALSE;

	int	len_buf, nbyte, len_line;
	int	ofset;

	is_first = is_first_indicate;

	/* set from previous buffer	*/
	if ( is_first ) {	/* when no buffer */
		is_first = FALSE;
		nbyte = readline ( fd, buf );
		len_line = (nbyte <= 2 ? 0 : nbyte - 2); buf[len_line] = '\0';
		len_buf = len_line;
	} else {
		len_buf = ( strlen(prev_rec) );
		memcpy( buf, prev_rec, len_buf ); buf[len_buf] = '\0';
		if ( is_last ) {
			*prev_rec = '\0';	/* set length of prev_rec to zero */
			is_last = FALSE, is_first = TRUE;
			return( 0 );
		}
	}

	/* read current record	*/
	for(;;) {
		nbyte = readline ( fd, r_line );
		len_line = (nbyte <= 2 ? 0 : nbyte - 2);
		if ( ( len_line == 0 ) ||				/* EOF */
		     ( r_line[0] == '.' && r_line[1] != '.')  ||	/* EOT */
		     ( r_line[0] != ' ' ) ) {	/* EOL */
			/* set previous buffer */
			memcpy( prev_rec,r_line, len_line ); prev_rec[len_line] = '\0';
			buf[len_buf] = '\0';
			if ( ( r_line[0] == '.' && r_line[1] != '.') || 
			      ( strlen( r_line ) <= 2 ) ) {
				is_last = TRUE;
				len_buf = 0;
			}
			return( /* len_buf */ strlen( buf ) );
		}

		memcpy( buf + len_buf, r_line + 1, len_line - 1 );
		len_buf += (len_line - 1);
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	get_boundary() - same as the name of function			*
 *									*
 *----------------------------------------------------------------------*/
int	get_boundary( char *buf, char *boundary )
{
	/* Content-Type: multipart/mixed; boundary="=============================102331843129542" */
	int	pos;
	char	*pos_boundary;
	int	ix;

	pos_boundary = strstr( buf, "boundary=" );
	if ( pos_boundary == NULL ) {
		*boundary = '\0';
		return(0);
	}

	pos_boundary += 9;
	if ( *pos_boundary == '\"' ) {
		pos_boundary++;
	}

	ix = 0;
	while( pos_boundary[ix] != '\0' && pos_boundary[ix] != '\"' ) {
		boundary[ix] = pos_boundary[ix];
		ix++;
	}
	boundary[ix] = '\0';
	return( 0 );
}
/*----------------------------------------------------------------------*
 *									*
 *	get_mime_type() - same as the name of function			*
 *									*
 *----------------------------------------------------------------------*/
int	get_mime_type( char *buf, char *mime_type )
{
	/* Content-Type: multipart/mixed; boundary="=============================102331843129542" */
	int	pos;
	char	*pos_mime_type;
	int	ix;

	pos_mime_type = buf + 14;
	if ( pos_mime_type == NULL ) {
		*mime_type = '\0';
		return(0);
	}
	ix = 0;
	while( pos_mime_type[ix] != '\0' && pos_mime_type[ix] != ';' ) {
		mime_type[ix] = pos_mime_type[ix];
		ix++;
	}
	mime_type[ix] = '\0';
	return( 0 );
}

/*----------------------------------------------------------------------*
 *									*
 *	is_boundary() - same as the name of function			*
 *									*
 *----------------------------------------------------------------------*/
Boolean	is_boundary( char *str, char *boundary )
{
	int	len_boundary;

	len_boundary = strlen( boundary );

	if ( ( strncmp( str, "--", 2 ) == 0 ) &&
	     ( strncmp( str + 2, boundary, len_boundary ) == 0 ) ) { 
		if ( str[2 + len_boundary] == '\r' ||
	             str[2 + len_boundary] == '\n' ||
	             str[2 + len_boundary] == '\0' ||
	             str[2 + len_boundary] == ' ' ) {
			return( TRUE );
		} else {
			return( FALSE );
		}
	} else {
		return( FALSE );
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	is_last_boundary() - same as the name of function		*
 *									*
 *----------------------------------------------------------------------*/
Boolean	is_last_boundary( char *str, char *boundary )
{
	int	len_boundary;

	len_boundary = strlen( boundary );

	if ( ( strncmp( str, "--", 2 ) == 0 ) &&
	     ( strncmp( str + 2, boundary, len_boundary ) == 0 ) &&
	     ( strncmp( str + 2 + len_boundary, "--", 2 ) == 0 ) ) {
		if ( str[4 + len_boundary] == '\r' ||
		     str[4 + len_boundary] == '\n' ||
		     str[4 + len_boundary] == '\0' ||
		     str[4 + len_boundary] == ' ' ) {
			return( TRUE );
		} else {
			return( FALSE );
		}
	} else {
		return( FALSE );
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	get_atatch_filename() - same as the name of function		*
 *									*
 *----------------------------------------------------------------------*/
int	get_attach_filename( char *buf, char *filename )
{
	/* Content-Type: application/octet-stream; name="profile.html"; */
	int	pos;
	char	*pos_filename;
	int	ix;

	pos_filename = strstr( buf, "name=" );
	if ( pos_filename == NULL ) {
		*filename = '\0';
		return(0);
	}

	pos_filename += 5;
	if ( *pos_filename == '\"' ) {
		pos_filename++;
	}

	ix = 0;
	while( pos_filename[ix] != '\0' && pos_filename[ix] != '\"' ) {
		filename[ix] = pos_filename[ix];
		ix++;
	}
	filename[ix] = '\0';
	return( 0 );
}

/*----------------------------------------------------------------------*
 *									*
 *	retrieve_each_head() - retrieve header 				*
 *									*
 *----------------------------------------------------------------------*/
int	retrieve_each_head( Boolean is_display, int fd,
			    char *subject,char *from,char *date,char *mime_type,char *boundary, char *filename )
{
	unsigned char	buf[HUGE];
	char		buf1[HUGE];
	unsigned char	buf2[HUGE];
	char	encoded_filename[BUFSIZ];
	int	nbyte, len_buf1;

	if ( subject   != NULL ) { *subject   = '\0'; };
	if ( from      != NULL ) { *from      = '\0'; };
	if ( date      != NULL ) { *date      = '\0'; };
	if ( mime_type != NULL ) { *mime_type = '\0'; };
	if ( boundary  != NULL ) { *boundary  = '\0'; };
	if ( filename  != NULL ) { *filename  = '\0'; };

	/* Process reply */
	nbyte = retrieve_header_line( fd, buf, TRUE );
	while( nbyte > 0 ) {
		/* process buffer	*/
		if (  boundary != NULL
		      && *boundary == '\0'
		      && strncmp( buf, HEADER_CONTENT_TYPE, strlen(HEADER_CONTENT_TYPE) ) == 0 ) {
			get_boundary( buf, boundary );
		}
		if ( mime_type != NULL
		     && *mime_type == '\0'
		      && strncmp( buf, HEADER_CONTENT_TYPE, strlen(HEADER_CONTENT_TYPE) ) == 0 ) {
			get_mime_type( buf, mime_type );
		}
		if ( filename != NULL
		     && *filename == '\0'
		     && strncmp( buf, HEADER_CONTENT_DISPOSITION, strlen(HEADER_CONTENT_DISPOSITION) ) == 0 ) {
			get_attach_filename( buf, encoded_filename );
			decode_str_of_mime ( encoded_filename, nbyte, buf1 );
			fmt_into_txt ( buf1, strlen(buf1), filename );
		}

		decode_str_of_mime ( buf, nbyte, buf1 );
		fmt_into_txt ( buf1, strlen(buf1), buf2 );
		if ( is_display ) {
			mon_print( TRUE, "%s\n", buf2 );
		}

		if ( subject != NULL
		     && *subject == '\0'
		     && strncmp ( buf2, HEADER_SUBJECT, strlen(HEADER_SUBJECT) ) == 0 ) {
			memcpy ( subject, &(buf2[9]), strlen(buf2) - 9 );
			subject[strlen(buf2) - 9] = '\0';
		} else if ( from != NULL
		            && *from == '\0'
		            && strncmp ( buf2, HEADER_FROM, strlen(HEADER_FROM) ) == 0 ) {
			memcpy ( from, &(buf2[6]), strlen(buf2) - 6 );
			from[strlen(buf2) - 6] = '\0';
		} else if ( date != NULL
		            && *date == '\0'
		            && strncmp ( buf2, HEADER_DATE, strlen(HEADER_DATE) ) == 0 ) {
			get_mail_date( buf2, date );
		}
		nbyte = retrieve_header_line( fd, buf, FALSE );
	}
}

/*------------------------------------------------------------------------------*
 *										*
 *	retrieve_related_message() - retrieve contents of alternative message	*
 *										*
 *------------------------------------------------------------------------------*/
int	retrieve_related_message ( int fd, char* boundary )
{
	int	nbyte;
	unsigned char	buf[HUGE];
	unsigned char	buf1[HUGE];
	unsigned char	buf2[HUGE];

	char	mime_type[BUFSIZ],filename[BUFSIZ];

	/* Main Header */
	for(;;) {
		if ( *boundary != '\0' && is_boundary( r_line,boundary ) ) {
			retrieve_each_head( FALSE, fd, NULL, NULL,NULL,  mime_type, NULL, filename );

/*
			if ( strncmp( mime_type, "text/html", 9 ) == 0 ) {
			} else {
			}
*/
		} else if ( is_last_boundary( r_line,boundary ) ) {
			return( 0 );
		}
		if ( is_eom(r_line) ) {	/* EOT */
			return( 0 );
		}
		if ( ( nbyte = readline ( fd, r_line ) ) <= 0 ) {
			return( 0 );
		}
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	retrieve_proper_mail() - retrieve mail 				*
 *									*
 *----------------------------------------------------------------------*/
int	retrieve_proper_mail ( int fd )
{
	int	nbyte;
	unsigned char	buf[HUGE];
	unsigned char	buf1[HUGE];
	unsigned char	buf2[HUGE];
	int	in_attach;

	char	mime_type[BUFSIZ],boundary[BUFSIZ],filename[BUFSIZ];
	char	boundary2[BUFSIZ];	/* For alternative message */

	/* Main Header */
	*boundary = '\0';
	retrieve_each_head( TRUE, fd, NULL, NULL, NULL, mime_type, boundary, NULL );

	if ( is_eom(r_line) ) {	/* EOT */
		return( 0 );
	}

	in_attach = FALSE;
	for(;;) {
		if (  *boundary != '\0' && is_boundary( r_line,boundary ) ) {
			retrieve_each_head( FALSE, fd, NULL, NULL,NULL,  mime_type, boundary2, filename );
			if ( strncmp( mime_type, "text/plain", 10 ) == 0 ) {
				in_attach = FALSE;
			} else if ( strncmp( mime_type, "multipart/related", 17 ) == 0 ) {
				mon_print( !(is_dump), "<B>This mail contains alternative(s)(perhaps HTML)</B>\n" );
				(void) retrieve_related_message( fd, boundary2 );
				if ( ( nbyte = readline ( fd, r_line ) ) <= 0 ) {
					return( 0 );
				}
				in_attach = FALSE;
			} else {
				in_attach = TRUE;
				mon_print( !(is_dump), "<B>Attached File: %s</B>\n", filename );
			}
		}
		if ( is_last_boundary( r_line,boundary ) ) {
			/* ignore this line */
		} else {
			if ( !( in_attach ) ) {
				strip_dummy_period( r_line );
				nbyte = nbyte - 1;
				fmt_into_txt ( r_line, nbyte, buf2 );
				mon_print( !(is_dump), "%s", buf2 );
			}
		}

		if ( is_eom(r_line) ) {	/* EOT */
			return( 0 );
		}
		if ( ( nbyte = readline ( fd, r_line ) ) <= 0 ) {
			return( 0 );
		}
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	dump_proper_mail() - retrieve mail by dump mode			*
 *									*
 *----------------------------------------------------------------------*/
int	dump_proper_mail ( int fd )
{
	int	nbyte;

	nbyte = readline ( fd, r_line );
	while( ( nbyte = readline ( fd, r_line ) ) > 0 ) {
		mon_print( TRUE, "%s", r_line );
		if ( is_eom(r_line) ) {	/* EOT */
			return( 0 );
		}
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	retrieve_mail() - retrieve prefer mail ( all when no. = 0 )	*
 *									*
 *----------------------------------------------------------------------*/
int	retrieve_mail ( int fd, int no, int mail_cnt )
{
	int	ix, max_ix, min_ix;
	char	boundary[HUGE];
	int	nbyte;

	Boolean	is_ok = FALSE;

	max_ix = (no != 0 ? no : mail_cnt);
	min_ix = (no != 0 ? no : 1       );

	for ( ix = min_ix; ix <= max_ix; ix++) {
		mon_print( !(is_dump), "<HR>\n" );
		sprintf ( s_line, "RETR %d\r\n", ix );
		fflush  ( fp_mon );
		mon_print( !(is_dump), "<FONT COLOR=blue>%s</FONT><BR>\n", s_line );
		is_ok = FALSE;
		nbyte = write_to_sock ( fd, s_line );

		if ( is_dump ) {
			dump_proper_mail( fd );
		} else {
			mon_print( !(is_dump), "<P><PRE>" );mon_flush( !(is_dump) );
			retrieve_proper_mail( fd );
			mon_print( TRUE, "</PRE>" );
			mon_print( TRUE, "<INPUT TYPE=checkbox NAME=R%d>Re-retrieve this mail ", ix );
			mon_print( TRUE, "<INPUT TYPE=checkbox NAME=D%d>Delete this mail</P>\n", ix );
		}

		mon_flush( !(is_dump) );
	}
}

/*----------------------------------------------------------------------*
 *									*
 *	retrieve_head() - retrieve prefer header ( all when no. = 0 )	*
 *									*
 *----------------------------------------------------------------------*/
int	retrieve_head ( int fd, int no, int mail_cnt )
{
	int	ix, max_ix, min_ix;
	char	from[BUFSIZ], subject[BUFSIZ],date[BUFSIZ],mime_type[BUFSIZ],boundary[BUFSIZ];
	int	nbyte;

	mon_print( TRUE, " <FORM NAME=f METHOD=POST ACTION=\"%s\">", cginame );
	mon_print( TRUE, "<TABLE BORDER=1 WIDTH=\"100%%\"><TR>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>Read</FONT></B></CENTER></TD>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>Delete</FONT></B></CENTER></TD>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>Date</FONT></B></CENTER></TD>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>Subject</FONT></B></CENTER></TD>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>From</FONT></B></CENTER></TD>\n" );
	mon_print( TRUE, "<TD><FONT COLOR=GREEN><B><CENTER>Size</FONT></B></CENTER></TD></TR>\n" );

	max_ix = (no != 0 ? no : mail_cnt);
	min_ix = (no != 0 ? no : 1       );

	for ( ix = min_ix; ix <= max_ix; ix++) {
		/* Send command */
		sprintf ( s_line, "TOP %d 1\r\n", ix );
		nbyte = write_to_sock ( fd, s_line );
		fflush  ( fp_mon );

		/* get replay */
		retrieve_each_head( FALSE, fd, subject, from, date, mime_type, NULL, NULL );
		while( !( r_line[0] == '.' && r_line[1] != '.' ) ) {	/* discard rest of message */
			(void) readline( fd, r_line );
		}

		mon_print( TRUE, "<TR><TD><INPUT TYPE=checkbox NAME=R%d></TD>\n", ix );
		mon_print( TRUE, "<TD><INPUT TYPE=checkbox NAME=D%d></TD>\n", ix );
		mon_print( TRUE, "<TD>%s</TD><TD>%s</TD><TD>%s</TD>", date, subject, from );
		if ( ix <= len_mail_size ) {
			mon_print( TRUE, "<TD>%d</TD></TR>\n", mail_size[ix - 1] );
		} else {
			mon_print( TRUE, "<TD></TD></TR>\n" );
		}
		mon_print( TRUE, "</TR>\n" );

	}

	mon_print( TRUE, "</TABLE><BR>\n" );
	print_javascript_indicate();
	display_input_area( );
	mon_print( TRUE, "</FORM>\n" );
	fflush  ( fp_mon );
}


/*----------------------------------------------------------------------*
*									*
 *	delete_mail() - delete prefer header 				*
 *									*
 *----------------------------------------------------------------------*/
int	delete_mail ( int fd, int no, int mail_cnt )
{
	/* Connect by user */
	sprintf ( s_line, "DELE %d\r\n", no );
	if ( !( input_cmd ( fd, s_line, r_line ) ) ) {
		return ( -1 );
	}
}


/*----------------------------------------------------------------------*
 *									*
 *	main() - main control						*
 *									*
 *----------------------------------------------------------------------*/

main(int argc, char *argv[])
{
	int	len;
	int	mail_cnt;
	char	*method;
	int	ix;
	char	*str_portno;
	int	portno;

	/* trap	*/
	atexit( on_end );

	/* Initial Process	*/
	fd = -1;
	pgmname = argv[0];
	cginame = get_cginame( pgmname );
	fp_mon = open_mon();
	signal( SIGINT, on_intr );
	signal( SIGHUP, on_intr );
	query_parm( NULL /* = set parameter from CGI environment */ );

	/* select method		*/
	if ( *(real_parm(F_METHOD)) == FM_INIT ) {
		display_initial_panel();
		exit ( 0 );
	}
	is_dump = ( *(real_parm(F_METHOD)) == FM_DUMP );	/* Set mode(Dump/other) */

	/* print out header information	*/
#ifdef	USR_NPH_MODE
	mon_print ( "HTTP/1.0 200 OK\n" );
#endif
	mon_print( TRUE, "Content-type: text/%s\n\n", (is_dump ? "plain" : "html" ) );
	mon_print( !(is_dump), "<HTML><HEAD><TITLE>Web Based POP3 Service - Execution view\n" );
	mon_print( !(is_dump), "</TITLE></HEAD><BODY BGCOLOR=WHITE><CENTER><FONT SIZE=+2>\n" ) ;
	mon_print( !(is_dump), "Web Based POP3 Service - result</FONT></CENTER>\n" );
	if ( !(is_dump ) ) {
		display_copyright( "RIGHT" );
	}
	mon_print( !(is_dump), "<p><FONT COLOR=green>------ execution log -------</FONT></p>\n" );
	mon_print( !(is_dump), "<p><FONT COLOR=green>execution date : %s</FONT></p>\n", get_date_str() );
	mon_flush( !(is_dump) );

	/* Decide a port number for POP3 */
	str_portno = real_parm( F_PORTNO );
	portno = str2int( str_portno );
	if ( portno <= 0 ) {
		portno = 110;
	}

	/* Connet to remote host */
	if ( ( connect_to_remote_host ( real_parm(F_HOST), portno ) ) < 0 ) {
		mon_print( !(is_dump), "<FONT COLOR=RED>Process terminated.</FONT><BR>\n");
		fflush  ( fp_mon );
		exit ( 0 );
	}

	/* login to mail server	*/
	if ( ( login_to_mail_server ( fd ) ) != 0 ) {
		exit ( 0 );
	}

	/* Retrieve number of mail	*/
	mail_cnt = retr_mail_list( fd ) ;

	/* Get mails	*/
	switch ( *(real_parm(F_METHOD)) ) {
	case FM_CONSOLE: 
	case FM_DUMP:
		mon_print( !(is_dump), " <FORM NAME=f METHOD=POST ACTION=\"%s\">\n", cginame );
		retrieve_mail( fd, 0, mail_cnt );
		if ( !(is_dump) ) {
			display_input_area( ) ;
		}
		mon_print( !(is_dump), "</FORM>\n" );
		break;
	case FM_HEAD:		/* retrieve head     */
		print_js_select_all_mail();
		retrieve_head( fd, 0, mail_cnt );
		break;
	case FM_PROPER:
		mon_print( !(is_dump), " <FORM NAME=f METHOD=POST ACTION=\"%s\">\n", cginame );
		for ( ix = 1; ix <= mail_cnt; ix++) {
			if ( is_mail_to_process( ix, 'R' ) ) {
				retrieve_mail( fd, ix, mail_cnt );
			}
		}
		for ( ix = 1; ix <= mail_cnt; ix++) {
			if ( is_mail_to_process( ix, 'D' ) ) {
				delete_mail( fd, ix, mail_cnt );
			}
		}
		if ( !is_dump ) {
			display_input_area( ) ;
		}
			mon_print( !(is_dump), "</FORM>\n" );
		fflush  ( fp_mon );
		break;
	default:
		break;
	}

	/* QUIT */
	input_cmd ( fd, "QUIT", r_line );
	alarm( WAITING );
	shutdown( fd, 2 );
	close ( fd );
	alarm( 0 );

	mon_print( !(is_dump), "<FONT COLOR=green>Connection closed. Goodbye.<BR></FONT>\n" );
	mon_print( !(is_dump), "<p><FONT COLOR=green>------ end of execution log -----</FONT></p>\n" );

	exit ( 0 );
}


