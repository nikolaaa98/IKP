#define WIN32_LEAN_AND_MEAN
#pragma warning( disable : 4996)

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "conio.h"
#include <time.h>
#include <stdint.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma pack(1)

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 27016
#define BUFFER_SIZE 256

typedef struct Interval {
	short gornji;
	short donji;
}INTERVAL;

typedef struct threadparameter {
	SOCKET* socket;
	sockaddr_in serverAddress;
	INTERVAL interval;
	int* index;
} T_PARAM;

int guessingNumber;

int srednja_vrednost(int donja, int gornja) {
	return (donja + gornja) / 2;
}

int random_broj(int donja, int gornja) {
	srand((unsigned)time(NULL));
	return (rand() % (gornja - donja + 1) + donja);
}

int random_broj_koji_se_trazi(int donja, int gornja) {
	srand((unsigned)time(NULL));
	return (rand() % (gornja - donja + 1) + donja);
}

DWORD WINAPI ThreadMainPlayer(LPVOID lpParam)
{
	T_PARAM* param = (T_PARAM*)lpParam;
	char dataBuffer[BUFFER_SIZE];
	int iResult;
	int receivedNumber = -1;
	bool correct = false;

	SOCKET acceptedSocket = *(param->socket);

	unsigned long  mode = 1;
	if (ioctlsocket(acceptedSocket, FIONBIO, &mode) != 0)
		printf("ioctlsocket failed with error.");

	//printf("New server request accepted. Server address: %s : %d\n", inet_ntoa(param->serverAddress.sin_addr), ntohs(param->serverAddress.sin_port));
	printf("Igraci pogadjaju broj %d \n", guessingNumber);

	while (!correct)
	{
		while (1)
		{
			///Primi broj koji je potrebno proveriti
			iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);    //ovaj recv ovde ne radi

			if (iResult > 0)
			{
				 dataBuffer[iResult] = '\0';
				 //printf("Proveravam broj: %s...\n", dataBuffer);			
				 receivedNumber = atoi(dataBuffer);
				 break;				
			}
			else if (iResult == 0)	// Check if shutdown command is received
			{
				// Connection was closed successfully
				printf("Connection with server closed.\n");
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

		if (receivedNumber == guessingNumber)
		{
			strcpy(dataBuffer, "tacno");
			correct = true;
		}
		else if (receivedNumber < guessingNumber)
		{
			strcpy(dataBuffer, "vece");
		}
		else
		{
			strcpy(dataBuffer, "manje");
		}

		///Saljem moj odgovor
		// Send message to server using connected socket
		iResult = send(acceptedSocket, dataBuffer, (int)strlen(dataBuffer), 0);

		// Check result of send function
		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedSocket);
			WSACleanup();
			return 1;
		}

		//printf("Poslao sam odgovor %s za broj %d.\n", dataBuffer, receivedNumber);
	}

	printf("\n***************************\nIgra je gotova\n***************************\n");

	_getch();
	return 0;
}

DWORD WINAPI ThreadSidePlayer(LPVOID lpParam)
{
	T_PARAM* param = (T_PARAM*)lpParam;
	char dataBuffer[BUFFER_SIZE];
	int iResult;
	INTERVAL interval;
	int num;
	
	SOCKET acceptedSocket = *(param->socket);

	//printf("New server request accepted. Server address: %s : %d\n", inet_ntoa(param->serverAddress.sin_addr), ntohs(param->serverAddress.sin_port));
	printf("Moj broj je %d \n", *(param->index));

	unsigned long  mode = 1;
	if (ioctlsocket(acceptedSocket, FIONBIO, &mode) != 0)
		printf("ioctlsocket failed with error.");

	while (1)
	{
		///Primam inicijalni interval od servera
		iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

		if (iResult > 0)
		{
			dataBuffer[iResult] = '\0';
			//printf("Primio sam interval: %s.\n", dataBuffer);			
			printf("Igra pocinje\n");
			char* token = strtok(dataBuffer, "-");
			interval.donji = atoi(token);
			token = strtok(NULL, "-");
			interval.gornji = atoi(token);
			break;
		}
		else if (iResult == 0)	// Check if shutdown command is received
		{
			// Connection was closed successfully
			printf("Connection with server closed.\n");
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
	
	printf("Broj je u intervalu: %d-%d\n", interval.donji, interval.gornji);

	while (1)
	{
		if (*(param->index) == 1)
		{
			num = srednja_vrednost(interval.donji, interval.gornji);
		}
		else
		{
			switch (*(param->index) / 4)
			{
			case 1:
				num = random_broj(interval.donji, interval.gornji) + 7;
				break;
			case 2:
				num = random_broj(interval.donji, interval.gornji) - 7;
				break;
			case 3:
				num = random_broj(interval.donji, interval.gornji) + 5;
				break;
			case 4:
				num = random_broj(interval.donji, interval.gornji) - 5;
				break;
			default:
				num = random_broj(interval.donji, interval.gornji) - 4;
				break;
			}
		}

		snprintf(dataBuffer, BUFFER_SIZE, "%d", num);

		///Saljem moj pokusaj
		// Send message to server using connected socket
		iResult = send(acceptedSocket, dataBuffer, (int)strlen(dataBuffer), 0);

		// Check result of send function
		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(acceptedSocket);
			WSACleanup();
			return 1;
		}

		printf("Moj predlog je: %s\n", dataBuffer);

		while (1)
		{
			///Cekam odgovor za svoj predlog
			iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);

			if (iResult > 0)
			{
				//dataBuffer2[iResult] = '\0';
				break;
			}
			else if (iResult == 0)	// Check if shutdown command is received
			{
				// Connection was closed successfully
				printf("Connection with server closed.\n");
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

		if (!strcmp(dataBuffer, "vece"))
		{
			printf("Server kaze treba mi %s.\n", dataBuffer);
			interval.donji = num;
		}
		else if (!strcmp(dataBuffer, "manje"))
		{
			printf("Server kaze treba mi %s.\n", dataBuffer);
			interval.gornji = num;
		}
		else if (!strcmp(dataBuffer,"tacno"))
		{
			printf("Pogodio sam tacan broj!!\n");
			break;
		}
		else
		{
			break;
		}
	}

	printf("\n***************************\nIgra je gotova\n***************************\n");
	_getch();
	return 0;
}

// TCP client that use non-blocking sockets
int main()
{
	// Socket used to communicate with server
	SOCKET connectSocket = INVALID_SOCKET;

	// Variable used to store function return value
	int iResult;

	// Buffer we will use to store message
	char dataBuffer[BUFFER_SIZE];

	// WSADATA data structure that is to receive details of the Windows Sockets implementation
	WSADATA wsaData;

	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return 1;
	}

	// create a socket
	connectSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Create and initialize address structure
	sockaddr_in serverAddresstemp;
	serverAddresstemp.sin_family = AF_INET;								// IPv4 protocol
	serverAddresstemp.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);	// ip address of server
	serverAddresstemp.sin_port = htons(SERVER_PORT);					// server port

	// Connect to server specified in serverAddress and socket connectSocket
	iResult = connect(connectSocket, (SOCKADDR*)&serverAddresstemp, sizeof(serverAddresstemp));
	if (iResult == SOCKET_ERROR)
	{
		printf("Unable to connect to server.\n");
		closesocket(connectSocket);
		WSACleanup();
		return 1;
	}
	//printf("Konekcija uspesna\n");

	int index;
	int32_t ret;
	char *data = (char*)&ret;
	guessingNumber = -1;

	T_PARAM threadParameter;

	DWORD ThreadMainPlayerID;
	DWORD ThreadSidePlayerID;

	HANDLE hMainPlayer = NULL;
	HANDLE hSidePlayer = NULL;

	iResult = recv(connectSocket, data, sizeof(ret), 0);
	if (iResult > 0)
	{
		index = ntohl(ret);
		//printf("Nova poruka pristigla od servera  : %d\n", index);
	}

	if (index == 0)
	{
		///Slanje intervala od strane igraca koji zamislja broj
		printf("Unesite interval u kom se zamisljeni broj nalazi u formatu donjaGranica-gornjaGranica\n");
		scanf("%s", &dataBuffer);

		// Send message to server using connected socket
		iResult = send(connectSocket, dataBuffer, (int)strlen(dataBuffer), 0);

		// Check result of send function
		if (iResult == SOCKET_ERROR)
		{
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(connectSocket);
			WSACleanup();
			return 1;
		}

		char* token = strtok(dataBuffer, "-");
		short donja_granica = atoi(token);
		token = strtok(NULL, "-");
		short gornja_granica = atoi(token);
		guessingNumber = random_broj_koji_se_trazi(donja_granica, gornja_granica);

		threadParameter.interval.donji = donja_granica;
		threadParameter.interval.gornji = gornja_granica;

		threadParameter.socket = &connectSocket;
		threadParameter.serverAddress = serverAddresstemp;
		hMainPlayer = CreateThread(NULL, 0, &ThreadMainPlayer, &threadParameter, 0, &ThreadMainPlayerID);
	}
	else
	{		
		printf("Uspesno ste se registrovali za pocetak igre. Molimo vas da sacekate, nova igra ce uskoro poceti...\n");
		threadParameter.socket = &connectSocket;
		threadParameter.serverAddress = serverAddresstemp;
		threadParameter.index = &index;
		hSidePlayer = CreateThread(NULL, 0, &ThreadSidePlayer, &threadParameter, 0, &ThreadSidePlayerID);
	}


	// Shutdown the connection since we're done
	Sleep(1000000);

	if (index == 0)
	{
		CloseHandle(hMainPlayer);
	}
	else
	{
		CloseHandle(hSidePlayer);
	}

	iResult = shutdown(connectSocket, SD_BOTH);

	// Check if connection is succesfully shut down.
	if (iResult == SOCKET_ERROR)
	{
		printf("Shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(connectSocket);
		WSACleanup();
		return 1;
	}

	// Close connected socket
	closesocket(connectSocket);

	// Deinitialize WSA library
	WSACleanup();
	
	return 0;
}
