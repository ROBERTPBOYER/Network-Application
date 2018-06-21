/*                                                                                                              #THE KILLER BUNNY
Robert Boyer                                                                                                         (\_/)
CSS 432 - Program 5 (Winter 2016)                                                                                    (o o)
-------------------------------------------------------------------------------------------------------------       (     )
Creation Date: Downloaded (2/28/2017)
Date of Last Modification:
--------------------------------------------------------------------------------------------------------------------------------
Purpose: This is an individual project to implement a network application: an ftp client program based on the Internet Engineering Task Force RFC 959 protocol.
--------------------------------------------------------------------------------------------------------------------------------
Notes: The file transfer protocol (ftp) is defined in a 1985 IETF [plain text] document, RFC 959. It is highly recommended that you read this document, and
understand the FTP functions (described in Section 4.1), especially: USER, PASS, SYST, PASV, LIST, RETR, STOR, QUIT
--------------------------------------------------------------------------------------------------------------------------------
*/
#include <iostream>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <netinet/in.h>   // htonl, htons, inet_ntoa
#include <arpa/inet.h>    // inet_ntoa
#include <netdb.h>        // gethostbyname
#include <unistd.h>       // read, write, close
#include <string.h>       // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <sys/uio.h>      // writev
#include <sys/poll.h> 	  // poll
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string>
#include <fstream>
#include <iomanip>
#include "Timer.h"

using namespace std;
bool debug = true;						//Toggle debug statements on/off
bool connection = false;			//If connection is established
int sd;												//Client Socket descriptor
int fd; 											//Client File descriptor
char *hostName;			  				//Name of the host
int hostPort = 21;						//Default port value
struct hostent *host;

//-------------------(Passive mode)-----------------

int dataSd;  // ls
int dataSd2; // get
int dataSd3; // put

/* 														a1	a2	a3 	a4	p1	p2
  227 Entering Passive Mode (209.202.252.54.xxx.xxx)
*/
int a1, a2, a3, a4; //IP address
int p1, p2;					//Port number(s)

//--------------------(Functions)-------------------

int socketPoll(int sd_1);
int connectToLogin();

//--------------------------------------------------

int main(int argc, char* argv[]) {

	bool autoConnect = false;			//If automatic connection should be established: i.e. atleast hostname was provided

	//------------------------------------------------------- (Argument Validation) ------------------------------------------------------------------

	//Provided NO arguments
	/* then enters Main logic loop and must enter command 'open' or 'quit' */

	//Provided too many arguments
	if (argc > 3){
		cerr << "usage: ftp host-name [port]" << endl;
	}

	//Provided only 1 argument: hostname implies default port of 21
	if (argc == 2){
		hostName = argv[1];
		autoConnect = true;
	}

	//Provided 2 arguments: hostname and port
	if (argc == 3){

		//Validate port #
		if (atoi(argv[2]) <= 0){
			cerr << "ftp: connect: Connection refused" << endl;
		}
		else{
			hostName = argv[1];
			hostPort = atoi(argv[2]);
			autoConnect = true;
		}
	}

	// If a hostname and valid port has been provided call autoConnect()
	if (autoConnect){
		if (connectToLogin() == 0){ //Attempt connect and login
			connection = true; //if success
		}
	}

	//-------------------------------------------------------- (Main Logic Loop) ----------------------------------------------------------------------

	do {
		cout << "ftp> ";

		string command;
		cin >> command;

		char file[30]; // put & get
		char remote[30]; //put
		char folder[30]; //cd

		//-------------------------------------------------------- (help) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "help"){
			cout << "Commands may be abbreviated. Commands are: " << endl;
			cout << "" << endl;
			cout << "cd" << endl;
			cout << "close" << endl;
			cout << "get" << endl;
			cout << "ls" << endl;
			cout << "open" << endl;
			cout << "put" << endl;
			cout << "quit" << endl;
			cout << "" << endl;
			continue;
		}

		//-------------------------------------------------------- (open) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "open"){ //No hostname or port # was provided: No connection established, then ok to start one

			if(connection == false){ //no existing connection
				cout << "(to) ";

				//Get user's hostName ---> ftp.tripod.com
				char hostn[14];
				bzero(hostn, sizeof(hostn));
				cin >> hostn;
				hostName = hostn;

				//Attempt connect & login
				if (connectToLogin() == 0){
					connection = true; //if successfully connected & logged in
				}
			}
			else{ // connection already exists
				cout << "Already connected to " << hostName << ", use close first." << endl;
			}
			continue;
		}

		//--------------------------------------------------------- (cd) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "cd" && connection == true){ //Must have connection established

			//Prompt & Read subdir
			cout << "(remote-directory) ";
			cin >> folder;

			//Build (CWD subdir\r\n)
			char dir[30];
			bzero(dir, sizeof(dir));
			strcpy(dir, "CWD ");
			strcat(dir, folder);
			strcat(dir, "\r\n");

			//Send (CWD subdir\r\n)
			write(sd, (char*)&dir, strlen(dir));

			//Receive acknowledgement
			char data[500];
			bzero(data, sizeof(data));
			read(sd, (char*)&data, sizeof(data));
			cout << data; //Print response: 250 Directory set to '/subdir'.
			continue;
		}

		//--------------------------------------------------------- (ls) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "ls" && connection == true){ //Must have connection established

			//Build (PASV\r\n)                                       													//-----------> Ftp::sendServerCommand <----------
			char passive[30];
			bzero(passive, sizeof(passive));
			strcpy(passive, "PASV");
			strcat(passive, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&passive, strlen(passive));            												//-------------------------------------------------

			//Receive acknowledgement 																											//-----------> Ftp::receiveServerReply <-----------
			char temp[100];
			bzero(temp, sizeof(temp));
			read(sd, (char*)&temp, sizeof(temp));
			cout << temp;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x) 			 	//-------------------------------------------------

			//Save IP address & port number(s)
			sscanf(temp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &a1, &a2, &a3, &a4, &p1, &p2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int servPort = (p1 * 256) + p2;

			//Build Passive sending socket address 																					//-------------> Ftp::createSocket <--------------
			struct sockaddr_in servAddr2;
			bzero((char*)&servAddr2, sizeof(servAddr2));
			servAddr2.sin_family = AF_INET;
			memcpy(&servAddr2.sin_addr, host->h_addr, host->h_length);
			servAddr2.sin_port = htons(servPort);

			//Open a TCP socket
			dataSd = socket(AF_INET, SOCK_STREAM, 0);
			if(dataSd < 0){
					cout << "Problem creating socket" << endl;
					exit(0);
			}

			//Connect this TCP socket with the server passively
			int returnCode = connect(dataSd, (struct sockaddr*)&servAddr2, sizeof(servAddr2));
			if(returnCode < 0){
					cout << "Problem connecting to server" << endl;
					close(dataSd);
					exit(0);
			}																																								//-------------------------------------------------


			//============================================================================================================

			int status;
			int pid = fork();
			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0){ // child

					//Collect Directory List
					char temp2[8192];
					bzero(temp2, sizeof(temp2));
					string f_dir_list = "";

					int length = 0;
					int total = 0;

					while (socketPoll(dataSd) > 0){

						bzero(temp2, sizeof(temp2));
						length = read(dataSd, temp2, sizeof(temp2));
						if (length == 0){
								break;
						}

						f_dir_list.append(temp2);

						total += length;
					}
					/*
					while(read(dataSd, (char*)&temp2, sizeof(temp2)) > 0){
																																				//cout << "read() = " << read(dataSd, (char*)&temp2, sizeof(temp2)) << endl;
																																			//cout << "temp2: " << temp2 << endl;
							f_dir_list.append(temp2);
					}
					*/																																//cout << "Directory List" << endl;
					//Print response: Directory List
					cout << f_dir_list << endl;

					//Close Socket
					close(dataSd);
					exit(EXIT_SUCCESS);
			}
			else{ // parent

					//Build (LIST\r\n)
					char list[10];
					bzero(list, sizeof(list));
					strcpy(list, "LIST");
					strcat(list, "\r\n");

					//Send (LIST\r\n)
					int sentList = write(sd, (char*)&list, strlen(list));

					//If successfully received
					if (sentList > 0){
																																											//cout << "feedback 1" << endl;
							//Receive acknowledgement
							char feedback[200];
							bzero(feedback, sizeof(feedback));
							read(sd, (char*)&feedback, sizeof(feedback));
							cout << feedback; //Print response: 150 Opening ASCII mode data connection for LIST.

							wait(&status); //Wait for child to finish
																																										//cout << "feedback 2" << endl;
							//Receive acknowledgement
							bzero(feedback, sizeof(feedback));
							read(sd, (char*)&feedback, sizeof(feedback));
							cout << feedback; //Print response: 226 Transfer complete. (x bytes sent.)
					}
			}

			//============================================================================================================

			continue;
		}

		//--------------------------------------------------------- (get) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "get" && connection == true){ //Must have connection established

			//Prompt & Read filename
			cout << "(remote-file) ";
			cin >> file;

			//Build (TYPE I\r\n)
			char type_i[12];
			bzero(type_i, sizeof(type_i));
			strcpy(type_i, "Type I");
			strcat(type_i, "\r\n");

			//Send (TYPE I\r\n)
			write(sd, (char*)&type_i, strlen(type_i));

			//Receive acknowledgement
			char verify[100];
			bzero(verify, sizeof(verify));
			read(sd, (char*)&verify, sizeof(verify));

			string msg = "200"; //Reply Code = The requested action has been successfully completed
			string response = verify; //Set response to string for comparison

			//If response string doesn't contain 200 Reply Code, then stop
			if(!strstr(response.c_str(), msg.c_str())) {
				cerr << "Error setting up binary mode" << endl;
				continue;
			}

			//Build (PASV\r\n)
			char passive[30];
			bzero(passive, sizeof(passive));
			strcpy(passive, "PASV");
			strcat(passive, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&passive, strlen(passive));

			//Receive acknowledgement
			char temp[100];
			bzero(temp, sizeof(temp));
			read(sd, (char*)&temp, sizeof(temp));

			cout << temp;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x)

			//Save IP address & port number(s)
			sscanf(temp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &a1, &a2, &a3, &a4, &p1, &p2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int servPort = (p1 * 256) + p2;

			//Build Passive sending socket address
			struct sockaddr_in servAddr3;
			bzero((char*)&servAddr3, sizeof(servAddr3));
			servAddr3.sin_family = AF_INET;
			servAddr3.sin_port = htons(servPort);
			memcpy(&servAddr3.sin_addr, host->h_addr, host->h_length);

			//Open a TCP socket
			dataSd2 = socket(AF_INET, SOCK_STREAM, 0);
			if(dataSd2 < 0) {
			 cout << "Error creating socket" << endl;
			 break;
			}

			//Connect this TCP socket with the server passively
			int returnCode = connect(dataSd2, (struct sockaddr*)&servAddr3, sizeof(servAddr3));
			if(returnCode < 0){
				cout << "Problem connecting to server" << endl;
				close(dataSd2);
				break;
			}

			cout << verify; // 200 Type set to 'I' (IMAGE aka Binary)

			//Build (RETR filename\r\n)
			char retrive[10];
			bzero(retrive, sizeof(retrive));
			strcpy(retrive, "RETR ");
			strcat(retrive, file);
			strcat(retrive, "\r\n");

			//Send (RETR filename\r\n)
			write(sd, (char*)&retrive, strlen(retrive));

			//Receive acknowledgement
			char feedback[200];
			bzero(feedback, sizeof(feedback));
			read(sd, (char*)&feedback, sizeof(feedback));

			cout << feedback;  //Print response: 150 Opening BINARY mode data connection for 'filename'.

			string msg2 = "150"; //Reply Code = File status okay; about to open data connection.
			string response2 = feedback; //Set response to string for comparison

			//If response string doesn't contain 150 Reply Code, then stop
			if(!strstr(response.c_str(), msg.c_str())) {
				cerr << "Error setting up file retrieval " << endl;
				close(dataSd2);
				continue;
			}

			mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
			fd = open(file, O_WRONLY | O_CREAT, mode);

			if (fd < 0){

				cerr << "local: " << file << ": Permission denied" << endl;
				close(dataSd2);
				close(fd);
				fd = -1;
				break;
			}

			Timer timer;
			double time = 0;

			int status;
			int pid = fork();

			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0) { // child

				timer.Start();

				char buffer[8192];
				int received = 0;
				int total = 0;

				while (socketPoll(dataSd2) > 0){

				// receive data in buffered chunks
				bzero(buffer, sizeof(buffer));
				received = read(dataSd2, buffer, sizeof(buffer));
				if (received == 0){
					break;
				}
					// write to disk before receiving the next chunk
					lseek(fd, 0, SEEK_END);
					write(fd, buffer, received);

					total += received;
				}

				time = timer.End();

				close(dataSd2); //Close passive data socket
				close(fd); //Close file descriptor
				fd = -1;

				//Receive acknowledgement
				bzero(feedback, sizeof(feedback));
				read(sd, (char*)&feedback, sizeof(feedback));
				cout << feedback;  //Print response: 226 Transfer complete. (x bytes sent.)

				//Caclulate Time
				if(debug){
					double secs = time / 1000000;
					cout << fixed << total << " bytes received in " <<
					setprecision(2) << secs << " secs (" <<
					setprecision(4) << total / secs / 1024 << " kB/s)" << endl;
				}

				exit(EXIT_SUCCESS);
			}
			else { // parent
					wait (&status);
			}

			close(dataSd2); //Close passive data socket
			continue;
		}

		//--------------------------------------------------------- (put) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "put" && connection == true){ //Must have connection established

			//Prompt & Read filename
			cout << "(local-file) ";
			cin >> file;
			cout << "(remote-file) ";
			cin >> remote;

			fd = open(file, O_RDONLY);
			if(fd < 0){
				cerr << "local: " << file << ": No such file or directory" << endl;
				continue;
			}

			//Build (TYPE I\r\n)
			char type_i[12];
			bzero(type_i, sizeof(type_i));
			strcpy(type_i, "Type I");
			strcat(type_i, "\r\n");

			//Send (TYPE I\r\n)
			write(sd, (char*)&type_i, strlen(type_i));

			//Receive acknowledgement
			char verify[100];
			bzero(verify, sizeof(verify));
			read(sd, (char*)&verify, sizeof(verify));

			string msg = "200"; //Reply Code = The requested action has been successfully completed
			string response = verify; //Set response to string for comparison

			//If response string doesn't contain 200 Reply Code, then stop
			if(!strstr(response.c_str(), msg.c_str())) {
				cerr << "Error setting up binary mode" << endl;
				continue;
			}

			//Build (PASV\r\n)
			char passive[30];
			bzero(passive, sizeof(passive));
			strcpy(passive, "PASV");
			strcat(passive, "\r\n");

			//Send (PASV\r\n)
			write(sd, (char*)&passive, strlen(passive));

			//Receive acknowledgement
			char temp[100];
			bzero(temp, sizeof(temp));
			read(sd, (char*)&temp, sizeof(temp));

			cout << temp;  //Print response: 227 Entering Passive Mode (x,x,x,x,x,x)

			//Save IP address & port number(s)
			sscanf(temp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &a1, &a2, &a3, &a4, &p1, &p2);

			//Compute Port Number: [(5th element * 256) + 6th element]
			int servPort = (p1 * 256) + p2;

			//Build Passive sending socket address
			struct sockaddr_in servAddr4;
			bzero((char*)&servAddr4, sizeof(servAddr4));
			servAddr4.sin_family = AF_INET;
			servAddr4.sin_port = htons(servPort);
			memcpy(&servAddr4.sin_addr, host->h_addr, host->h_length);

			//Open a TCP socket
			dataSd3 = socket(AF_INET, SOCK_STREAM, 0);
			if(dataSd3 < 0) {
			 cout << "Error creating socket" << endl;
			 exit(0);
			}

			//Connect this TCP socket with the server passively
			int returnCode = connect(dataSd3, (struct sockaddr*)&servAddr4, sizeof(servAddr4));
			if(returnCode < 0){
				cout << "Problem connecting to server" << endl;
				close(dataSd3);
				break;
			}

			cout << verify; // 200 Type set to 'I' (IMAGE aka Binary)

			//Build (STOR filename\r\n)
			char stor[100];
			bzero(stor, sizeof(stor));
			strcpy(stor, "STOR ");
			strcat(stor, file);
			strcat(stor, "\r\n");

			//Send (STOR filename\r\n)
			write(sd, (char*)&stor, strlen(stor));

			//Receive acknowledgement
			char feedback[500];
			bzero(feedback, sizeof(feedback));
			read(sd, (char*)&feedback, sizeof(feedback));

			cout << feedback;  //Print response: 150 Opening BINARY mode data connection for 'filename'.

			string msg2 = "150"; //Reply Code = File status okay; about to open data connection.
			string response2 = feedback; //Set response to string for comparison

			//If response string doesn't contain 150 Reply Code, then stop
			if(!strstr(response.c_str(), msg.c_str())) {
				cerr << "Error setting up file storage " << endl;
				close(dataSd3);
				continue;
			}

			Timer timer;
			double time = 0;
			long fileSize = 0;

			int status;
			int pid = fork();

			if (pid < 0){
					cerr << "Error: Fork failed" << endl;
					exit(EXIT_FAILURE);
			}
			else if (pid == 0){ // child

				timer.Start();

				// get length of file
				int length = lseek(fd, 0, SEEK_END);
				lseek(fd, 0, 0); //reset to beginning

				char *buffer = new char[length];

				// read data as a block
				read(fd, buffer, length);

				// transmit the buffer
				int fileSize = write(dataSd3, buffer, length);

				delete[] buffer;
				buffer = NULL;


				time = timer.End();

				shutdown(dataSd3, SHUT_WR);
				close(dataSd3);

				close(fd);
				fd = -1;

				//Receive acknowledgement
				char feedback2[500];
				bzero(feedback2, sizeof(feedback2));
				read(sd, (char*)&feedback2, sizeof(feedback2));
				cout << feedback2; //Print response: 226 Transfer complete. (x bytes sent.)

				double secs = time / 1000000;
				cout << fixed << fileSize << " bytes sent in " <<
				setprecision(2) << secs << " secs (" <<
				setprecision(4) << fileSize / secs / 1024 << " kB/s)" << endl;

				exit(EXIT_SUCCESS);
			}
			else { // parent
					wait (&status);
			}

			close(dataSd3); //Close passive data socket
			continue;
		}

		//-------------------------------------------------------- (close) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "close" && connection == true){ //Must have connection established

			//Build (QUIT\r\n)
			char quit[10];
			bzero(quit, sizeof(quit));
			strcpy(quit, "QUIT");
			strcat(quit, "\r\n");

			//Send (QUIT\r\n)
			write(sd, (char*)&quit, strlen(quit));

			//Receive acknowledgement
			char temp[20];
			bzero(temp, sizeof(temp));
			read(sd, (char*)&temp, sizeof(temp));
			cout << temp; //Print response: 221 Goodbye...

			shutdown(sd, SHUT_WR);
			connection = false;
			continue; //continue ftp
		}

		//-------------------------------------------------------- (quit) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		if(command == "quit"){

			//Build (QUIT\r\n)
			char quit[10];
			bzero(quit, sizeof(quit));
			strcpy(quit, "QUIT");
			strcat(quit, "\r\n");

			//Send (QUIT\r\n)
			write(sd, (char*)&quit, strlen(quit));

			//Receive acknowledgement
			char temp[20];
			bzero(temp, sizeof(temp));
			read(sd, (char*)&temp, sizeof(temp));
			cout << temp; //Print response: 221 Goodbye...
			break; //exit ftp
		}

		//----------------------------------------------------- (non-valid) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		else{ //Non-valid commands

			if(connection == false){ //Not connected
				cout << "Not connected." << endl;
				continue;
			}
			else{ //Unrecognizable command
				cout << "?Invalid command" << endl;
				continue;
			}
		}

	} while (true);

	close(sd);
	return 0;
}

//-------------------------------------------------------- (Connect to Login) --------------------------------------------------------------------

// Function (1) establishes a connection to specified server (2) completes user login & password
int connectToLogin(){

	cout << "Connected to " << hostName << endl;

	// Get Hostent structure to communicate to server.
	host = gethostbyname(hostName);
	if (host == NULL){
		cerr << "ftp:  "<< hostName << " Name or service not known"<< endl;
		return -1;
	}

	// Build the sending socket address of the client.
	struct sockaddr_in addr;
	bzero((char *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*) (*host->h_addr_list)));
	addr.sin_port = htons(hostPort);

	// Open a TCP socket.
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0){
		cerr << "Problem creating socket" << endl;
		close(sd);
		return -1;
	}

	// Connect this TCP socket with the server
	int returnCode = connect(sd, (sockaddr *)&addr, sizeof(addr));
	if (returnCode < 0){ //negative return indicates failure
		cerr << "Problem connecting to server" << endl;
		close(sd);
		return -1;
	}

	//Read from server & print response: 220 Welcome to Tripod us FTP.
	char buffer[1500];
	bzero(buffer, sizeof(buffer));
	read(sd, (char*)&buffer, sizeof(buffer));
	cout << buffer;

	//Prompt & Read user_name
	string userString(getlogin()); //To display system username
	cout << "Name ("<< hostName << ":" << userString << "): ";
	char name[9]; // (css432w17)
	bzero(name, sizeof(name));
	cin >> name;

	//Build (USER user_name\r\n)
	char user[16]; // (USER css432w17\r\n)
	bzero(user, sizeof(user));
	strcpy(user, "USER ");
	strcat(user, name);
	strcat(user, "\r\n");

	write(sd, (char*)&user, strlen(user)); //Send (USER user_name\r\n)
	bzero(buffer, sizeof(buffer));
	read(sd, (char*)&buffer, sizeof(buffer)); //Receive acknowledgement
	cout << buffer; //Print response: 331 Username set to (user_name). Now enter your password.

	//Prompt & Read user_password
	cout << "Password: ";
	char pword[9]; // UWB0th3ll = 9
	bzero(pword, sizeof(pword));
	cin >> pword;

	//Build (PASS user_password\r\n)
	char pass[16];
	bzero(pass, sizeof(pass));
	strcpy(pass, "PASS ");
	strcat(pass, pword);
	strcat(pass, "\r\n");

	write(sd, (char*)&pass, strlen(pass)); //Send (PASS user_password\r\n)
	bzero(buffer, sizeof(buffer));
	read(sd, (char*)&buffer, sizeof(buffer)); //Receive acknowledgement

	string error = "501"; //Reply Code = Syntax error in parameters or arguments
	string response = buffer; //Set response to string for comparison

	//If response string doesn't contain 501 Reply Code, password accepted
	if(!strstr(response.c_str(), error.c_str())){

		cout << buffer;

		//Reads for more data: IMPORTANT NOTICE section
		while(socketPoll(sd) > 0){
			bzero(buffer, sizeof(buffer));
			read(sd, (char*)&buffer, sizeof(buffer));
			cout << buffer << endl;
		}
		//Unable to Read data
		if(socketPoll(sd) == -1){
			cerr << "Error polling the socket" << endl;
		}

		//Build (SYST \r\n)
		char sys[30];
		bzero(sys, sizeof(sys));
		strcpy(sys, "SYST ");
		strcat(sys, "\r\n");

		//Send (SYST \r\n)
		write(sd, (char*)&sys, strlen(sys));

		//Receive acknowledgement
		char data[500];
		bzero(data, sizeof(data));
		read(sd, (char*)&data, sizeof(data));
		cout << data; //Print response: 215 UNIX Type: L8

		return 0;
	}
	else{ //501
		//Expected output following: $ftp ftp.tripod.com example
		cout << "Login failed." << endl;
		return -1;
	}

}

//----------------------------------------------------------- (SocketPoll) -----------------------------------------------------------------------

// Function checks socket descriptor for data and return a positive number if readable, otherwise returns 0 or a negative number
int socketPoll(int sd_1) {

    struct pollfd ufds;
    ufds.fd = sd_1;             	// a socket descriptor to exmaine for read
    ufds.events = POLLIN; 				// check if this sd is ready to read
    ufds.revents = 0;						// simply zero-initialized

    return poll(&ufds, 1, 1000); 		//poll this socket for 1000msec
}
