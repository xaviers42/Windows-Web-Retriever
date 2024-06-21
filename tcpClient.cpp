/*
* tcpClient.cpp : DICT.org program.
* Author: Xavier Sherman
* Class: CSCI 344
* Date: March 26, 2019
*/

#define WIN32_LEAN_AND_MEAN

#include "pch.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

#define DEFAULT_BUFFER 512
#define DEFAULT_PORT "2628"
#define QUIT_STATUS_CODE 221
#define INVALID_DB 550
#define NO_MATCH 552
#define DICTIONARY_LOOKUP 1
#define GAZ_LOOKUP 2

using namespace std;
char *newStr;
char *completeCmdStr;

int wsaDescriptor;
SOCKET socket_t = INVALID_SOCKET;
struct addrinfo *addrInfo = NULL, hints;

int statustoi(char *);
char* preparestr(char *);
void sys_shutdown();
char* appendcmd(int, char*);

int main()
{
	char userInput[DEFAULT_BUFFER];
	WSADATA wsaData;
	char recvbuf[DEFAULT_BUFFER];
	int recvbuflen = DEFAULT_BUFFER;
	char quitMessage[] = "quit\n";

	// Start of socket setup.
	wsaDescriptor = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaDescriptor != 0)
		cout << "WSA initialization failed.\n";

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	wsaDescriptor = getaddrinfo("www.dict.org", DEFAULT_PORT, &hints, &addrInfo);
	if (wsaDescriptor != 0) {
		cout << "getaddr failed.\n";
		WSACleanup();
	}

	socket_t = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
	if (socket_t == INVALID_SOCKET) {
		cout << "Socket creation failed.\n";
		freeaddrinfo(addrInfo);
		WSACleanup();
	}

	wsaDescriptor = connect(socket_t, addrInfo->ai_addr, (int)addrInfo->ai_addrlen);
	if (wsaDescriptor == SOCKET_ERROR) {
		closesocket(socket_t);
		socket_t = INVALID_SOCKET;
		cout << "Cannot connect to server.\n";
		freeaddrinfo(addrInfo);
		WSACleanup();
	}
	//End of socket setup.

	/*
	* Clear the buffer of all its contents so its ready to receive. The buffer will be
	* filled with the server response message from trying to connect. Then the
	* message is printed to the screen.
	*/
	ZeroMemory(recvbuf, recvbuflen);
	wsaDescriptor = recv(socket_t, recvbuf, recvbuflen, 0);
	cout << recvbuf;

	do {

		cout << "Enter a command...\n";
		cin >> userInput;

		if ((strncmp(userInput, "d", 1) == 0) || (strncmp(userInput, "D", 1) == 0)) {

			cout << "Enter word to lookup: ";
			cin.ignore(); //Discard characters in cin buffer.
			cin.getline(userInput, sizeof(userInput));
			char *newString = appendcmd(DICTIONARY_LOOKUP, preparestr(userInput));
			send(socket_t, newString, strlen(newString), 0);
			ZeroMemory(recvbuf, recvbuflen);
			int found;


			do {

				ZeroMemory(recvbuf, recvbuflen);
				wsaDescriptor = recv(socket_t, recvbuf, recvbuflen, 0);
				string recvBufString(recvbuf);
				if (wsaDescriptor > 0) {
					puts(recvbuf);
				}
				else {
					if (wsaDescriptor == 0)
						cout << "done receiving " << WSAGetLastError();
					else
						cout << endl;
				}

				found = recvBufString.find("250", 0); //Search for a 250 status code.
				//found == -1 means the 250 was not found.
				if (found == -1) {
					if (statustoi(recvbuf) == INVALID_DB)
						found = -2;
					if (statustoi(recvbuf) == NO_MATCH)
						found = -2;
				}
			/*
			* Keep looping while a 250 status code is not found AND the number
			* and the number of bytes received is greater than zero. 
			*/
			} while ((found == -1) && (wsaDescriptor > 0));
			
			free(newStr); //The string is no longer needed after preparestr() returns.
		}
		else if ((strncmp(userInput, "z", 1) == 0) || (strncmp(userInput, "Z", 1) == 0)) {

			cout << "Enter ZIP code to lookup: ";
			cin.ignore(); //Discard characters in cin buffer.
			cin.getline(userInput, sizeof(userInput));
			char *newString = appendcmd(GAZ_LOOKUP, preparestr(userInput));
			send(socket_t, newString, strlen(newString), 0);
			ZeroMemory(recvbuf, recvbuflen);
			int found;

			do {

				wsaDescriptor = recv(socket_t, recvbuf, recvbuflen, 0);
				string recvBufString(recvbuf);
				if (wsaDescriptor > 0) {
					puts(recvbuf);
				}
				else {
					if (wsaDescriptor == 0)
						cout << "done receiving " << WSAGetLastError();
					else
						cout << endl;
				}

				found = recvBufString.find("250", 0); //Search for a 250 status code.
				if (found == -1) {
					if (statustoi(recvbuf) == INVALID_DB)
						found = -2;
					if (statustoi(recvbuf) == NO_MATCH)
						found = -2;
				}
			/*
			* Keep looping while a 250 status code is not found AND the number
			* and the number of bytes received is greater than zero.
			*/
			} while ((found == -1) && (wsaDescriptor > 0));

			free(newStr);
		}

	} while (!((strncmp(userInput, "q", 1) == 0) || (strncmp(userInput, "Q", 1) == 0)));

	/*
	* Send quit command to server because user decided to quit program.
	*/
	ZeroMemory(recvbuf, recvbuflen);
	wsaDescriptor = send(socket_t, quitMessage, strlen(quitMessage), 0);
	wsaDescriptor = recv(socket_t, recvbuf, recvbuflen, 0);

	if (statustoi(recvbuf) == QUIT_STATUS_CODE) {
		sys_shutdown();
	}
	else
		cout << "shutdown failed.\n";

	return 0;
}

/*
* Converts the server response string's status number
* into an integer for any type of comparison.
* Arg: status -- the server response string.
* Return: sum -- the status code.
*/
int statustoi(char *status) {

	int sum = 0;

	for (int i = 0; i < 3; i++) {
		if (i == 0)
			sum += ((int)status[i] - 48) * 10 * 10;
		if (i == 1)
			sum += ((int)status[i] - 48) * 10;
		if (i == 2)
			sum += (int)status[i] - 48;
	}
	return sum;
}

/*
* Appends the "CRLF" character squence to the given string.
* Arg: str -- string to which the squence is appended.
* Return: newStr -- string which has the appended squence.
*/
char* preparestr(char *str) {

	int pos = strlen(str);
	int newStrLen = pos + 3;
	newStr = (char*)malloc(newStrLen * sizeof(char));
	ZeroMemory(newStr, newStrLen);
	strncpy(newStr, str, pos);
	newStr[pos] = 13; // CR character
	newStr[pos + 1] = 10; // LF character
	return newStr;
}

/*
* Inserts the dictionary commands in front of userInput to complete
* the send request.
* Arg: cmdType -- an integer value denoting the type of dictionary lookup
* to the done.
* Arg: userInput -- query by user.
* Return: completeCmdStr -- string with the complete command attached.
*/
char* appendcmd(int cmdType, char *userInput) {
	int strSize;
	if (cmdType == DICTIONARY_LOOKUP) {
		strSize = strlen(userInput) + 10;
		completeCmdStr = (char *)malloc(strSize * sizeof(char));
		strcpy(completeCmdStr, "DEFINE wn ");
		strcat(completeCmdStr, userInput);
	}
	if (cmdType == GAZ_LOOKUP) {
		strSize = strlen(userInput) + 18;
		completeCmdStr = (char *)malloc(strSize * sizeof(char));
		strcpy(completeCmdStr, "DEFINE gaz2k-zips ");
		strcat(completeCmdStr, userInput);
	}

	return completeCmdStr;
}

/*
* Shutdown code which is ran after the user decides
* to quit the program.
*/
void sys_shutdown() {
	wsaDescriptor = shutdown(socket_t, SD_SEND);
	if (wsaDescriptor == SOCKET_ERROR) {
		cout << "shutdown failed. Error: " << WSAGetLastError() << endl;
		closesocket(socket_t);
		WSACleanup();
	}
	freeaddrinfo(addrInfo);
	closesocket(socket_t);
	WSACleanup();
}