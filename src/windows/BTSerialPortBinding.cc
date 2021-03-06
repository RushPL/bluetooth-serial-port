#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <string>
#include <stdlib.h>
#include "../BluetoothException.h"
#include "../BTSerialPortBinding.h"
#include "BluetoothHelpers.h"

struct bluetooth_data
{
	SOCKET s;
	bool initialized;
};

using namespace std;

BTSerialPortBinding *BTSerialPortBinding::Create(string address, int channelID)
{
	if (channelID <= 0)
		throw BluetoothException("ChannelID should be a positive int value");

	char addressBuffer[40];

	if (strcpy_s(addressBuffer, address.c_str()) != 0)
		throw BluetoothException("Address (first argument) length is invalid");

	auto obj = new BTSerialPortBinding(addressBuffer, channelID);

	if (!obj->data->initialized)
	{
		delete obj;
		throw BluetoothException("Unable to initialize socket library");
	}

	return obj;
}

BTSerialPortBinding::BTSerialPortBinding(char address[40], int channelID)
	: channelID(channelID)
{
	strcpy_s(this->address, address);
	data = new bluetooth_data();
	data->s = INVALID_SOCKET;
	data->initialized = BluetoothHelpers::Initialize();
}

BTSerialPortBinding::~BTSerialPortBinding()
{
	if (data->initialized)
		BluetoothHelpers::Finalize();
	delete data;
}

void BTSerialPortBinding::Connect()
{
	Close();

	int status = SOCKET_ERROR;

	data->s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);

	if (data->s != SOCKET_ERROR)
	{
		SOCKADDR_BTH addr = { 0 };
		int addrSize = sizeof(SOCKADDR_BTH);
		int addrErr = WSAStringToAddress(address, AF_BTH, nullptr, (LPSOCKADDR)&addr, &addrSize);

		if (addrErr != SOCKET_ERROR)
		{
			addr.port = channelID;
			status = connect(data->s, (LPSOCKADDR)&addr, addrSize);

			if (status != SOCKET_ERROR)
			{
				unsigned long enableNonBlocking = 1;
				ioctlsocket(data->s, FIONBIO, &enableNonBlocking);
			}
		}
	}

	if (status != 0)
	{
		if (data->s != INVALID_SOCKET)
			closesocket(data->s);

		throw BluetoothException("Cannot connect");
	}
}

void BTSerialPortBinding::Close()
{
	//NOTE: The address argument is currently only used in OSX.
	//      On windows each connection is handled by a separate object.

	if (data->s != INVALID_SOCKET)
	{
		closesocket(data->s);
		data->s = INVALID_SOCKET;
	}
}

int BTSerialPortBinding::Read(char *buffer, int offset, int length)
{
	if (data->s == INVALID_SOCKET)
		throw BluetoothException("connection has been closed");

	if (buffer == nullptr)
		throw BluetoothException("buffer cannot be null");

	if (length == 0)
		return 0;

	fd_set set;
	FD_ZERO(&set);
	FD_SET(data->s, &set);

	int size = -1;

	if (select(static_cast<int>(data->s) + 1, &set, nullptr, nullptr, nullptr) >= 0)
	{
		if (FD_ISSET(data->s, &set))
			size = recv(data->s, buffer + offset, length, 0);
		else // when no data is read from rfcomm the connection has been closed.
			size = 0; // TODO: throw ???
	}

	if (size < 0)
		throw BluetoothException("Error reading from connection");

	return size;
}

void BTSerialPortBinding::Write(char *buffer, int offset, int length)
{
	if (buffer == nullptr)
		throw BluetoothException("buffer cannot be null");

	if (length == 0)
		return;

	if (data->s == INVALID_SOCKET)
		throw BluetoothException("Attempting to write to a closed connection");

	if (send(data->s, buffer + offset, length, 0) != length)
		throw BluetoothException("Writing attempt was unsuccessful");
}