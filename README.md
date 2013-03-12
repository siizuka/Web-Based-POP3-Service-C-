Web-Based-POP3-Service-C-
=========================
Web Based POP3 Service Version4.63

                                         2002.04.07  IIZUKA Shinji
                                         (siizuka@nurs.or.jp)

NAME

    Web Based POP3 Service(wbpop.cgi) - Web based POP3 client


OVERVIEW

    Web Based POP3 Service is a POP3 clinent CGI program.
    Using this, you can retrieve/delete your mail outside firewall
    from your machine inside firewall.Both normal POP3 and APOP 
    protocol are aviable.

    This program can "view all mails", "view all headers of mails",
    "view mails you indicate","delete your mail".

    Using dump mode,displays bulk content of mailbox, you can
    download and save the result of this service and get attach
    files with a help of your mail software.


ENVIRONENT REQUIED

   This progarm require the environment following:

    - Host machine i.e:
      Linux,
      Solaris 2.5, 2.6,
      SunOS 5.1,
      IRIX
      Windows95 with Cygnus gcc
    - C compiler with socket library in general UNIX-syntax
      (i.e. gcc). Winsock is NOT aviable.
    - HTTP server i.e.
      Apache,
      CERN HTTPd,


Content of Archive

   wbpop_4.63-+- source ---+- wbpop.c               source program
              |            |
              |            +- md5.c                 MD5 library(RFC1321)
              |            |
              |            +- md5.h                 MD5 header file
              |            |                                   (RFC1321)
              |            +- makefile              makefile,so-called
              |
              +- doc.jis --+- readme.jis.txt        Japanese readme
              |            |
              |            +- install_wbpop.jis.txt Japanese install guide
              |            |
              |            +- man-wbpop-jis.html    Japanese manual(html)
              |
              +- doc.sjis ---- (same as doc.jis)
              |
              +- doc.eucj ---  (same as doc.jis)
              |
              +- readme.txt                         This file
              |
              +- install_wbpop.txt                  Install guide
              |
              +- man-wbpop.html                     Operation manual


Installation

    see install_wbpop.txt.


Author

      IIZUKA  Shinji(siizuka@nurs.or.jp)


BUGS

      Since this is _POP3_ client, IMAP4 is NOT aviable.

      This program resets connection of your mail server on each 
      request from blower, so you may retrieve/delete incorrect
      mail when other person (or process) delete mail(s) on your
      server between your first request and second one.

      Attach file NOT aviable(attach files are viewed as a part of
      mail itself). Use "Dump mode" and your mailer to get attach
      files from your mail.


CONDITION FOR USE

     MD5 library is based on sample source of RFC1321("The MD5 
     Message-Digest Algorithm"). See RFC to check out the condition
     for use of md5.*

     The copyright of this progarm Except the md5 library belong to
     IIZUKA Shinji.

     You admired to use/transplant this progarm for non-exclusive and
     non-commercial use. This progarm is provided free of charge for 
     aforementioned use.

     Any commercial use of this script without admmition of author is
     prohibited. "Commercial use" includes exhibiting this program for
     fee or use as banner site. Mail me(siizuka@nurs.or.jp)
     if you intend to use this script on commercial use.

     This program was tested, but this NOT means that this script
     includes no bugs. This program is served "AS IS" bases,so you must
     be chargeable to the entire risk of this program(i.e. the quality
     and/or performance).

     Formal document of all conditions is readme.sjis.txt,Japanese text.
     If there is/are mismatch(s) between readme.sjis.txt and this document,
     readme.sjis.txt is token precedence over this document.
------------------------- END OF DOCUMENT ------------------------------
