#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "conio.h"
#include <time.h>
#include <stdint.h>
#include <string.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma pack(1)

#define SERVER_PORT 27016
#define BUFFER_SIZE 256
#define MAX_CLIENTS 10

typedef struct Interval {
	short gornji;
	short donji;
}INTERVAL;

typedef struct pogodak {
	int vrednost;
	int idKlijenta;
	bool obradjen;
	char odgovor;
}POGODAK;

typedef struct cvor {
	POGODAK pogodak;
	struct cvor* sledeci;
}CVOR;

CVOR* head = NULL;

typedef struct threadparameter {
	SOCKET* socket;
	short* index;
	sockaddr_in clientAddr;
	LPVOID lpParam;
	int* br_cvorova;			
} T_PARAM;

HANDLE hSemaphores[MAX_CLIENTS];
HANDLE hSemaphoreMain;

CRITICAL_SECTION cs;

INTERVAL interval;
int currentNumberOfPlayers;
bool kraj;

CVOR* dodajCvor(int* value, int* clientID, int* count)
{
	if (*count == 0)												//dodavanje glave
	{
		CVOR* new_node = (CVOR*) malloc(sizeof(CVOR));

		new_node->pogodak.vrednost = *value;
		new_node->pogodak.idKlijenta = *clientID;
		new_node->pogodak.obradjen = false;
		new_node->sledeci = head;
		head = new_node;

		(*count)++;

		return new_node;
	}
	else
	{
		CVOR* noviCvor = (CVOR*)malloc(sizeof(CVOR));

		noviCvor->pogodak.vrednost = *value;
		noviCvor->pogodak.idKlijenta = *clientID;
		noviCvor->pogodak.obradjen = false;
		noviCvor->sledeci = head;
		head = noviCvor;

		(*count)++;

		return noviCvor;
	}
}

void ispisCvorova()
{
	CVOR* temp = head;

	printf("+++++++++++++++++++++++++++\n");
	int i = 1;

	while (temp != NULL)
	{
		printf("Vrednost:%d idKlijenta:%d pogodakObradjen:%d\n", temp->pogodak.vrednost, temp->pogodak.idKlijenta, temp->pogodak.obradjen);
		temp = temp->sledeci;
		i++;
	}
	printf("+++++++++++++++++++++++++++\n");
}

DWORD WINAPI AcceptThreadMainPlayer(LPVOID lpParam)
{
	T_PARAM* param = (T_PARAM*)lpParam;
	int iResult;
	char dataBuffer[BUFFER_SIZE];
	int* br_cvorova = param->br_cvorova;
	int obavestiKlijenta;

	// Struct for information about connected client
	SOCKET acceptedSocket = *(param->socket);	

	unsigned long  mode = 1;
	if (ioctlsocket(acceptedSocket, FIONBIO, &mode) != 0)
		printf("ioctlsocket failed with error.");
	
	printf("New client request accepted %d . Client address: %s : %d\n", *(param->index), inet_ntoa(param->clientAddr.sin_addr), ntohs(param->clientAddr.sin_port));
	while (1)
	{
		///Primanje pocetnog intervala
		iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);
		if (iResult > 0)	// Check if message is successfully received
		{
			dataBuffer[iResult] = '\0';
			//printf("Primio sam pocetni interval: %s.\n", dataBuffer);
			break;
		}
		else if (iResult == 0)	// Check if shutdown command is received
		{
			// Connection was closed successfully
			printf("Connection with client closed.\n");
			closesocket(acceptedSocket);
		}
		else	// There was an error during recv
		{
			int ierr = WSAGetLastError();
			if (ierr == WSAEWOULDBLOCK) // currently no data available
			{  
				Sleep(50);  // wait and try again
				continue;
			}
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedSocket);
		}
	}

	char* token = strtok(dataBuffer, "-");
	interval.donji = atoi(token);
	token = strtok(NULL, "-");
	interval.gornji = atoi(token);
	
	///Obavestavanje niti da salje klijentima interval
	ReleaseSemaphore(hSemaphores[0], 1, NULL);

	///Cekanje da stigne prvi broj koji treba obraditi
	WaitForSingleObject(hSemaphoreMain, INFINITE);
		
	while (!kraj)
	{
		if (!(head->pogodak.obradjen))
		{
				snprintf(dataBuffer, BUFFER_SIZE, "%d", head->pogodak.vrednost);

				///Slanje prvog broja klijentu koji je zamislio broj
				// Send message to client using connected socket
				iResult = send(acceptedSocket, dataBuffer, (int)strlen(dataBuffer), 0);

				// Check result of send function
				if (iResult == SOCKET_ERROR)
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(acceptedSocket);
					WSACleanup();
					return 1;
				}

				printf("Zelim da proverim broj: %d \n", head->pogodak.vrednost);

				while (1)
				{
					///Primanje odgovora o tom broju od klijenta koji je zamislio broj
					iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

					if (iResult > 0)	// Check if message is successfully received
					{
						dataBuffer[iResult] = '\0';
						// Log message text
						printf("Odgovor od glavnog igraca: %s.\n", dataBuffer);
						EnterCriticalSection(&cs);
						strcpy(&(head->pogodak.odgovor), dataBuffer);
						if(!strcmp(&(head->pogodak.odgovor),"tacno"))
							kraj = true;
						head->pogodak.obradjen = true;
						obavestiKlijenta = head->pogodak.idKlijenta;
						LeaveCriticalSection(&cs);
						break;
					}
					else if (iResult == 0)	// Check if shutdown command is received
					{
						// Connection was closed successfully
						printf("Connection with client closed.\n");
						closesocket(acceptedSocket);
					}
					else	// There was an error during recv
					{
						int ierr = WSAGetLastError();
						if (ierr == WSAEWOULDBLOCK) // currently no data available
						{
							Sleep(50);  // wait and try again
							continue;
						}
						printf("recv failed with error: %d\n", WSAGetLastError());
						closesocket(acceptedSocket);
					}
				}

			ReleaseSemaphore(hSemaphores[obavestiKlijenta], 1, NULL);
		}
		else
		{
			Sleep(50);
		}
		
	}
	
	printf("\n***************************\nIgra je gotova\n***************************\n");
	_getch();

	return 0;
}

DWORD WINAPI AcceptThreadSidePlayer(LPVOID lpParam)
{
	T_PARAM* param = (T_PARAM*)lpParam;
	char dataBuffer[BUFFER_SIZE];
	int iResult;
	int n = (int)(param->lpParam);
	int* br_cvorova = param->br_cvorova;
	CVOR* cvorZaKojiCekam;

	// Struct for information about connected client
	SOCKET acceptedSocket = *(param->socket);

	printf("New client request accepted %d . Client address: %s : %d\n", n+1, inet_ntoa(param->clientAddr.sin_addr), ntohs(param->clientAddr.sin_port));

	///Cekam da stigne interval koji mogu poslati svom klijentu
	WaitForSingleObject(hSemaphores[n], INFINITE);

	char interval_gornji[BUFFER_SIZE];
	char interval_donji[BUFFER_SIZE];
	snprintf(interval_gornji, BUFFER_SIZE, "%u", interval.gornji);
	snprintf(interval_donji, BUFFER_SIZE, "%u", interval.donji);

	strcat(interval_donji, "-");
	strcat(interval_donji, interval_gornji);

	strcpy(dataBuffer, interval_donji);

	// Send message to client using connected socket
	///Saljem interval svom klijentu
	iResult = send(acceptedSocket, dataBuffer, (int)strlen(dataBuffer), 0);

	// Check result of send function
	if (iResult == SOCKET_ERROR)
	{
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(acceptedSocket);
		WSACleanup();
		return 1;
	}

	///Obavestavam sledeceg da posalje svom klijentu
	ReleaseSemaphore(hSemaphores[(n + 1) % currentNumberOfPlayers], 1, NULL);	
	
	while (1)
	{
		///Cekam da na mene dodje red da primim predlog svog klijenta
		WaitForSingleObject(hSemaphores[n], INFINITE);

		while (1)
		{
			///Primam predlog mog klijenta
			iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

			if (iResult > 0)	// Check if message is successfully received
			{
				dataBuffer[iResult] = '\0';
				// Log message text
				//printf("Igrac %d je poslao predlog: %s.\n", n, dataBuffer);
				int value = atoi(dataBuffer);
				EnterCriticalSection(&cs);
				cvorZaKojiCekam = dodajCvor(&value, &n, br_cvorova);
				//printf("Broj cvorova trenutno: %d\n", *br_cvorova);
				LeaveCriticalSection(&cs);
				break;
			}
			else if (iResult == 0)	// Check if shutdown command is received
			{
				// Connection was closed successfully
				printf("Connection with client closed.\n");
				closesocket(acceptedSocket);
				return 1;
			}
			else	// There was an error during recv
			{
				int ierr = WSAGetLastError();
				if (ierr == WSAEWOULDBLOCK) // currently no data available
				{
					Sleep(50);  // wait and try again
					continue;
				}
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(acceptedSocket);
			}
		}

		if (!kraj)
		{
			///Obavesti main da proveri za novi cvor koji sam dodao
			//printf("Obavestio sam main da proveri novi cvor \n");
			ReleaseSemaphore(hSemaphoreMain, 1, NULL);
		}
		else
		{
			strcpy(dataBuffer, "kraj");

			///Posalji poruku klijentu da je kraj igre
			// Send message to client using connected socket
			iResult = send(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

			// Check result of send function
			if (iResult == SOCKET_ERROR)
			{
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(acceptedSocket);
				return 1;
			}

			break;
		}

		///Cekam da moj zahtev bude obradjen
		WaitForSingleObject(hSemaphores[n], INFINITE);		

		strcpy(dataBuffer, &(cvorZaKojiCekam->pogodak.odgovor));

		///Saljem odgovor svom igracu koji je poslao predlog
		// Send message to client using connected socket
		iResult = send(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

		// Check result of send function
		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedSocket);
			WSACleanup();
			return 1;
		}

		//printf("Odgovor klijent %d je %s.\n",n, &(cvorZaKojiCekam->pogodak.odgovor));

		///Obavesti sledecu nit da primi predlog svog klijenta
		ReleaseSemaphore(hSemaphores[(n + 1) % currentNumberOfPlayers], 1, NULL);

		if (kraj)
		{
			break;
		}
	}

	//printf("\n***************************\nIgra je gotova\n***************************\n");

	return 0;
}

// TCP server that use non-blocking sockets
int main()
{
	// Socket used for listening for new clients 
	SOCKET listenSocket = INVALID_SOCKET;

	// Sockets used for communication with client
	SOCKET acceptedSockets[MAX_CLIENTS];
	short lastIndex = 0;

	// Variable used to store function return value
	int iResult;

	// WSADATA data structure that is to receive details of the Windows Sockets implementation
	WSADATA wsaData;

	HANDLE hMainPlayer;
	HANDLE handleSidePlayer[MAX_CLIENTS];

	DWORD AcceptThreadMainPlayerID;
	DWORD sidePlayerID[MAX_CLIENTS];

	T_PARAM threadParameterMain;
	T_PARAM sidePlayerParameters[MAX_CLIENTS];

	InitializeCriticalSection(&cs);

	interval.gornji = -1;
	interval.donji = -1;
	kraj = false;

	int br_cvorova = 0;

	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return 1;
	}

	// Initialize serverAddress structure used by bind
	sockaddr_in serverAddress;
	memset((char*)&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;				// IPv4 address family
	serverAddress.sin_addr.s_addr = INADDR_ANY;		// Use all available addresses
	serverAddress.sin_port = htons(SERVER_PORT);	// Use specific port

	//initialise all client_socket[] to 0 so not checked
	memset(acceptedSockets, 0, MAX_CLIENTS * sizeof(SOCKET));

	// Create a SOCKET for connecting to server
	listenSocket = socket(AF_INET,      // IPv4 address family
		SOCK_STREAM,  // Stream socket
		IPPROTO_TCP); // TCP protocol

	// Check if socket is successfully created
	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket - bind port number and local address to socket
	iResult = bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

	// Check if socket is successfully binded to address and port from sockaddr_in structure
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	unsigned long  mode = 1;
	if (ioctlsocket(listenSocket, FIONBIO, &mode) != 0)
		printf("ioctlsocket failed with error.");

	// Set listenSocket in listening mode
	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	printf("Server socket is set to listening mode. Waiting for new connection requests.\n");

	// set of socket descriptors
	fd_set readfds;

	// timeout for select function
	timeval timeVal;
	timeVal.tv_sec = 1;
	timeVal.tv_usec = 0;

	while (true)
	{
		// initialize socket set
		FD_ZERO(&readfds);

		// add server's socket and clients' sockets to set
		if (lastIndex != MAX_CLIENTS)
		{
			FD_SET(listenSocket, &readfds);
		}
		
		for (int i = 0; i < lastIndex; i++)
		{
			FD_SET(acceptedSockets[i], &readfds);
		}

		// wait for events on set
		int selectResult = select(0, &readfds, NULL, NULL, &timeVal);

		if (selectResult == SOCKET_ERROR)
		{
			printf("Select failed with error: %d\n", WSAGetLastError());
			closesocket(listenSocket);
			WSACleanup();
			return 1;
		}
		else if (selectResult == 0) // timeout expired
		{
			if (_kbhit()) //check if some key is pressed
			{
				Sleep(100);
				printf("Select char");
				_getch();
				printf("Timeout expired\n");
			}
			continue;
		}
		else if (FD_ISSET(listenSocket, &readfds))
		{
			// Struct for information about connected client
			sockaddr_in clientAddrtemp;
			int clientAddrSize = sizeof(struct sockaddr_in);

			// New connection request is received. Add new socket in array on first free position.
			acceptedSockets[lastIndex] = accept(listenSocket, (struct sockaddr*)&clientAddrtemp, &clientAddrSize);

			if (acceptedSockets[lastIndex] == INVALID_SOCKET)
			{
				if (WSAGetLastError() == WSAECONNRESET)
				{
					printf("accept failed, because timeout for client request has expired.\n");
				}
				else
				{
					printf("accept failed with error: %d\n", WSAGetLastError());
				}
			}
			else
			{
				if (ioctlsocket(acceptedSockets[lastIndex], FIONBIO, &mode) != 0)
				{
					printf("ioctlsocket failed with error.");
					continue;
				}
				//printf("New client request accepted (%d). Client address: %s : %d\n", lastIndex, inet_ntoa(clientAddrtemp.sin_addr), ntohs(clientAddrtemp.sin_port));
				lastIndex++;
			

				short index = lastIndex - 1;
				currentNumberOfPlayers = index;
				int32_t sendIndex = htonl(index);
				char *data = (char*)&(sendIndex);

				//Send index to client
				iResult = send(acceptedSockets[index], data, (int)sizeof(data), 0);

				// Check result of send function
				if (iResult == SOCKET_ERROR)
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(acceptedSockets[index]);
					WSACleanup();
					return 1;
				}

				if (index == 0)
				{
					threadParameterMain.socket = &acceptedSockets[index];
					threadParameterMain.index = &(index);
					threadParameterMain.clientAddr = (clientAddrtemp);
					threadParameterMain.br_cvorova = &br_cvorova;
					//printf("\nThread main player created \n");
					hSemaphoreMain = CreateSemaphore(0, 0, 1, NULL);
					hMainPlayer = CreateThread(NULL, 0, &AcceptThreadMainPlayer, &threadParameterMain, 0, &AcceptThreadMainPlayerID);
					Sleep(500);
				}
				else
				{
					sidePlayerParameters[index].socket = &acceptedSockets[index];
					sidePlayerParameters[index].clientAddr = (clientAddrtemp);
					sidePlayerParameters[index].lpParam = (LPVOID)(index - 1);
					sidePlayerParameters[index].br_cvorova = &br_cvorova;
					//printf("\nThread side player created \n");
					hSemaphores[index - 1] = CreateSemaphore(0, 0, 1, NULL);
					handleSidePlayer[index] = CreateThread(NULL, 0, &AcceptThreadSidePlayer, &sidePlayerParameters[index], 0, &sidePlayerID[index]);
					Sleep(500);
				}
			}
		}
		
	}

	iResult = shutdown(listenSocket, SD_BOTH);

	// Check if connection is succesfully shut down.
	if (iResult == SOCKET_ERROR)
	{
		printf("Shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	CloseHandle(hMainPlayer);
	for (int i = 1; i < currentNumberOfPlayers; i++)
	{
		CloseHandle(handleSidePlayer[i]);
	}
	DeleteCriticalSection(&cs);

	//Close listen and accepted sockets
	closesocket(listenSocket);
	for (int i = 0; i < currentNumberOfPlayers; i++)
	{
		closesocket(acceptedSockets[i]);
	}

	// Deinitialize WSA library
	WSACleanup();

	return 0;
}
