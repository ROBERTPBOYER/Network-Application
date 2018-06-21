/*                                                                                                              #THE KILLER BUNNY
Robert Boyer                                                                                                         (\_/)
CSS 432 - Program 5 (Winter 2016)                                                                                    (o o)
-------------------------------------------------------------------------------------------------------------       (     )
Creation Date: 3/10/2017
Date of Last Modification:
--------------------------------------------------------------------------------------------------------------------------------
Purpose: This is an individual project to implement a network application: an ftp client program based on the Internet Engineering Task Force RFC 959 protocol.
--------------------------------------------------------------------------------------------------------------------------------
Notes: The file transfer protocol (ftp) is defined in a 1985 IETF [plain text] document, RFC 959. It is highly recommended that you read this document, and
understand the FTP functions (described in Section 4.1), especially: USER, PASS, SYST, PASV, LIST, RETR, STOR, QUIT
--------------------------------------------------------------------------------------------------------------------------------
*/
#ifndef _TIMER_H_
#define _TIMER_H_

#include <sys/time.h>
#include <iostream>
using namespace std;

class Timer{

    public:
        Timer();
        void Start();                 // Start time
        long End();               		// FInal time = endTime - startTime

    private:
        struct timeval startTime;
        struct timeval endTime;
};

#endif
