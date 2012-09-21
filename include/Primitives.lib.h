#pragma once

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <Windows.h>
#include <map>
#include <vector>
#include <sstream>
#include <memory>

//////////////////////////////
#pragma comment(lib, "Ws2_32")
#pragma comment(lib, "rpcrt4")

////////////////////////////////
inline void Verify(bool check) {
	if(!check)
		::DebugBreak();
}

//////////////
class TEvent {
private:
	HANDLE hEvent;
public:
	TEvent() : hEvent(NULL) {
		hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		Verify(NULL != hEvent);
	}
public:
	operator HANDLE() { return hEvent; }
public:
	~TEvent() {
		BOOL check = ::CloseHandle(hEvent);
		Verify(TRUE == check);
		hEvent = NULL;
	}
};

///////////////
class TNumber {
private:
	enum { reserve_capacity = 128 };
public:
	TNumber() : out_writer(reserve_capacity) {
		out_buffer.reserve(reserve_capacity);
	}
private:
	std::stringstream in_reader;
private:
	std::string in_buffer;
public:
	size_t /*end_offset*/ Read(std::vector<char> &buffer, size_t start_offset, __int64 *num) {
		in_buffer.clear();
		Verify(start_offset < buffer.size());
		size_t len = buffer[start_offset] - '0';
		start_offset++;
		Verify((start_offset + len) < buffer.size());
		in_buffer.insert(0, buffer.at(start_offset), buffer.at(start_offset + len));
		int val = 0;
		in_reader.str(in_buffer);
		in_reader >> val;
		return val;
	}
private:
	std::stringstream out_writer;
private:
	std::string out_buffer;
public:
	void Append(std::vector<char> &buffer, __int64 val) {
		out_writer.str("");
		out_writer << val;
		Verify(out_writer.str().length() >= 9);
		out_buffer.clear();
		out_buffer += static_cast<char>(val);
		out_buffer += out_writer.str();
		buffer.insert(buffer.end(), out_buffer.begin(), out_buffer.end());
	}
};

/////////////////////////////////////////////////////
class TMessage : std::map<std::string, std::string> {
private:
	TNumber number;
public:
	TMessage() { }
private:
	std::string key, val;
public:
	size_t Read(std::vector<char> &buffer, size_t offset) {
		__int64 count = 0;
		offset = number.Read(buffer, offset, &count);
		__int64 len_key = 0, len_val = 0;
		for(__int64 i = 0; i < count; i++) {
			len_key = 0;
			offset = number.Read(buffer, offset, &len_key);
			key.clear(); key.insert(key.begin(), buffer.at(offset), buffer.at(offset + static_cast<size_t>(len_key)));
			offset += static_cast<size_t>(len_key);
			len_val = 0;
			offset = number.Read(buffer, offset, &len_val);
			val.clear(); val.insert(val.begin(), buffer.at(offset), buffer.at(offset + static_cast<size_t>(len_val)));
			offset += static_cast<size_t>(len_val);
			(*this)[key] = val;
		}
		return offset;
	}
public:
	void Append(std::vector<char> &buffer) {
		number.Append(buffer, this->size());
		for(iterator iter = begin(); iter != end(); iter++) {
			const std::string &key = iter->first;
			number.Append(buffer, key.length());
			buffer.insert(buffer.end(), key.begin(), key.end());
			std::string &val = iter->second;
			number.Append(buffer, val.length());
			buffer.insert(buffer.end(), val.begin(), val.end());
		}
	}
public:
	void Dump(std::ostream out) {
		for(iterator iter = begin(); iter != end(); iter++) {
			out << iter->first << ": " << iter->second << std::endl;
		}
		out << std::endl;
	}
};

/////////////////////////
class TOverlapped;

/////////////////////////
class ICompletionResult {
public:
	virtual void Completed(BOOL status, DWORD byte_count, TOverlapped *overlapped) = 0;
};

///////////////
class ISocket {
public:
	virtual operator HANDLE() = 0;
public:
	virtual operator SOCKET() = 0;
public:
	virtual BOOL TransmitFile(HANDLE hFile,	DWORD nNumberOfBytesToWrite,
		DWORD nNumberOfBytesPerSend, LPOVERLAPPED lpOverlapped,
		LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers, DWORD dwReserved) = 0;
public:
	virtual BOOL AcceptEx(SOCKET sAcceptSocket,	PVOID lpOutputBuffer,
		DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
		LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped) = 0;
public:
	virtual void GetAcceptExSockAddrs(PVOID lpOutputBuffer,	DWORD dwReceiveDataLength,
		DWORD dwLocalAddressLength,	DWORD dwRemoteAddressLength, struct sockaddr **LocalSockaddr,
		LPINT LocalSockaddrLength, struct sockaddr **RemoteSockaddr, LPINT RemoteSockaddrLength) = 0;
public:
	virtual BOOL TransmitPackets(LPTRANSMIT_PACKETS_ELEMENT lpPacketArray,                               
		DWORD nElementCount, DWORD nSendSize, LPOVERLAPPED lpOverlapped,                  
		DWORD dwFlags) = 0;
public:
	virtual BOOL ConnectEx(const struct sockaddr FAR *name,
		int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength,
		LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped) = 0;
public:
	virtual BOOL DisconnectEx(LPOVERLAPPED lpOverlapped, DWORD  dwFlags, DWORD  dwReserved) = 0;
public:
	virtual BOOL Send(LPWSABUF lpBuffers, DWORD dwBufferCount, LPOVERLAPPED lpOverlapped) = 0;
public:
	virtual BOOL Recv(LPWSABUF lpBuffers, DWORD dwBufferCount, LPOVERLAPPED lpOverlapped) = 0;
public:
	virtual ~ISocket() { }
};

typedef std::shared_ptr<ISocket> ISocketPtr;

///////////////////////
class IIOCPEvented {
public:
	virtual void Attach(HANDLE hChild) = 0;
public:
	virtual void FlushQueue() = 0;
public:
	virtual void Run() = 0;
public:
	virtual void Stop() = 0;
public:
	virtual void FlushQueueEx() = 0;
};

typedef std::shared_ptr<IIOCPEvented> IIOCPEventedPtr;

