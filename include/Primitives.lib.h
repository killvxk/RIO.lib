#pragma once

// disable warnings not found in v110 toolset (VS2012)
#pragma warning(disable : 4355)
#pragma warning(disable : 4482)

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <Windows.h>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <memory>
#include <rpc.h>

//////////////////////////////
#pragma comment(lib, "Ws2_32")
#pragma comment(lib, "rpcrt4")

namespace MurmurBus {
	////////////////////////////////
	inline void Verify(bool check) {
		DWORD last_err = ::GetLastError();
		if(!check)
			::__debugbreak();
	}

	//////////////////////////////
	inline void NotImplemented() {
		::__debugbreak();
		throw std::exception("not implemented");
	}

	////////////////
	class TOneShot {
	private:
		bool flag;
	public:
		operator bool() {
			if(flag) {
				flag = false;
				return true;
			} else {
				return flag;
			}
		}
	public:
		TOneShot() : flag(true) { }
	};

	//////////////
	class IEvent {
	public:
		virtual operator HANDLE() = 0;
	public:
		virtual void WaitOne() = 0;
	public:
		virtual ~IEvent() { }
	};
	typedef std::shared_ptr<IEvent> IEventPtr;

	////////////////
	enum constants {
		max_msg_size = 1024,
		max_block_count = 65536
	};

	/////////////////////////////////
	typedef std::vector<char> TBytes;

	//////////////////////////////////////////////
	void Copy(TBytes &bytes, std::string &buffer) {
		buffer.clear();
		buffer.insert(buffer.begin(), bytes.begin(), bytes.end());
	}

	/////////////
	class TUUID {
	private:
		UUID uuid;
	public:
		TUUID() {
			UUID clear = { 0 };
			uuid = clear;
			RPC_STATUS status = ::UuidCreate(&uuid);
			Verify(RPC_S_OK == status);
		}
	public:
		operator UUID() { return uuid; }
	public:
		operator std::string() {
			RPC_CSTR buffer = NULL;
			RPC_STATUS status = ::UuidToStringA(&uuid, &buffer);
			Verify(RPC_S_OK == status);
			std::string out(reinterpret_cast<char*>(buffer));
			status = ::RpcStringFreeA(&buffer);
			Verify(RPC_S_OK == status);
			return out;
		}
	}; // TUUID

	//////////////
	class TEvent : public IEvent {
	private:
		HANDLE hEvent;
	public:
		TEvent() : hEvent(NULL) {
			hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			Verify(NULL != hEvent);
		}
	private:
		IEvent::operator HANDLE() { return hEvent; }
	private:
		void IEvent::WaitOne() {
			DWORD wait = ::WaitForSingleObject(hEvent, INFINITE);
			Verify(WAIT_OBJECT_0 == wait);
		}
	public:
		~TEvent() {
			BOOL check = ::CloseHandle(hEvent);
			Verify(TRUE == check);
			hEvent = NULL;
		}
	}; // TEvent

	///////////////
	class TNumber {
	private:
		enum { reserve_capacity = 128 };
	public:
		TNumber() {
			out_writer_cache.reserve(16);
			out_buffer.reserve(reserve_capacity);
		}
	private:
		std::string in_buffer;
	public:
		bool Read(std::string &buffer, size_t start_offset, size_t *end_offset, __int64 *num) {
			in_buffer.clear();
			if(start_offset >= buffer.size()) return false;
			size_t len = buffer[start_offset] - '0';
			start_offset++;
			if((start_offset + len) >= buffer.size()) return false;
			*end_offset = start_offset + len;
			in_buffer.insert(in_buffer.begin(), 
				buffer.begin() + start_offset, buffer.begin() + *end_offset);
			int val = 0;
			std::stringstream in_reader(in_buffer);
			in_reader >> val;
			*num = val;
			return true;
		}
	private:
		std::string out_buffer;
	private:
		std::string out_writer_cache;
	public:
		void Append(std::string &buffer, __int64 val) {
			out_writer_cache.clear();
			std::stringstream out_writer(out_writer_cache);
			out_writer << val;
			Verify(out_writer.str().length() <= 9);
			Verify(out_writer.str().length() > 0);
			out_buffer.clear();
			out_buffer += static_cast<char>(out_writer.str().length() + '0');
			out_buffer += out_writer.str();
			buffer.insert(buffer.end(), out_buffer.begin(), out_buffer.end());
		}
	}; // TNumber

	////////////// LiPS - Length Prefixed Strings //////////////
	class TMessage : public std::map<std::string, std::string> {
	private:
		TNumber number;
	public:
		TMessage() { }
	private:
		std::string key, val;
	public:
		bool Read(std::string &buffer, size_t start_offset, size_t *end_offset) {
			__int64 count = 0;
			if(!number.Read(buffer, start_offset, &start_offset, &count)) return false;
			__int64 len_key = 0, len_val = 0;
			for(__int64 i = 0; i < count; i++) {
				len_key = 0;
				if(!number.Read(buffer, start_offset, &start_offset, &len_key)) return false;
				key.clear(); key.insert(key.begin(), 
					buffer.begin() + start_offset, buffer.begin() + (start_offset + static_cast<size_t>(len_key)));
				start_offset += static_cast<size_t>(len_key);
				if(start_offset >= buffer.size()) return false;
				len_val = 0;
				if(!number.Read(buffer, start_offset, &start_offset, &len_val)) return false;
				val.clear(); val.insert(val.begin(), 
					buffer.begin() + start_offset, buffer.begin() + (start_offset + static_cast<size_t>(len_val)));
				start_offset += static_cast<size_t>(len_val);
				if(start_offset > buffer.size()) return false;
				(*this)[key] = val;
			}
			*end_offset = start_offset;
			return true;
		}
	public:
		void Append(std::string &buffer) {
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
		void SoftClear() {
			for(iterator iter = begin(); iter != end(); iter++)
				iter->second.clear();
		}
	public:
		void Merge(TMessage &merge) {
			for(auto iter = merge.begin(); iter != merge.end(); iter++) {
				(*this)[iter->first] = iter->second;
			}
		}
	public:
		bool TryGet(std::string &key, std::string &val) {
			iterator iter = find(key);
			if(iter->second.length() > 0) {
				val = iter->second;
				return true;
			} else {
				return false;
			}
		}
	public:
		void Dump(std::ostream &out) {
			for(iterator iter = begin(); iter != end(); iter++) {
				out << iter->first << ": " << iter->second << std::endl;
			}
			out << std::endl;
		}
	}; // TMessage
	typedef std::shared_ptr<TMessage> TMessagePtr;

	/////////////////////////
	class TOverlapped;

	/////////////////////////
	class ICompletionResult {
	public:
		virtual void Completed(BOOL status, DWORD byte_count, TOverlapped *overlapped) = 0;
	}; // ICompletionResult

	///////////////////////////////////////
	class TOverlapped : public OVERLAPPED {
		friend class TIOCP;
	public:
		void Reset() {
			HANDLE hEvent = this->hEvent;
			LPOVERLAPPED clear = this;
			memset(clear, 0, sizeof(OVERLAPPED));
			this->hEvent = hEvent;
		}
	private:
		TOverlapped();
	private:
		ICompletionResult *iCompletion;
	public:
		operator LPOVERLAPPED() { return this; }
	public:
		TOverlapped(ICompletionResult *iCompletion, HANDLE hEvent) : iCompletion(iCompletion) {
			this->hEvent = hEvent;
			Reset();
		}
	}; // TOverlapped

	/////////////
	class TIOCP {
	private:
		HANDLE hIOCP;
	public:
		TIOCP() {
			hIOCP = ::CreateIoCompletionPort(
				INVALID_HANDLE_VALUE, NULL, 0, 0);
			Verify(NULL != hIOCP);

			last_tick = ::GetTickCount64();
		}
	private:
		__int64 completion_count;
	private:
		unsigned __int64 last_tick;
	private:
		void DoStats() {
			completion_count++;
			if(completion_count > 10000) {
				unsigned __int64 tick = ::GetTickCount64();
				double rate = static_cast<double>(completion_count * 1000);
				rate /= static_cast<double>(tick - last_tick);
				std::cout << rate << " completions/s" << std::endl;
				last_tick = tick;
				completion_count = 0;
			}
		}
	public:
		void Attach(HANDLE hChild) {
			HANDLE hCheck = ::CreateIoCompletionPort(
				hChild, hIOCP, 0, 0);
			Verify(NULL != hCheck);
		}
	private:
		bool FlushQueue() {
			LPOVERLAPPED lpOverlapped = NULL;
			ULONG_PTR completion_key = 0;
			DWORD byte_count = 0;
			BOOL status = ::GetQueuedCompletionStatus(
				hIOCP, &byte_count, &completion_key, &lpOverlapped, 0);
			TOverlapped *pOverlapped = reinterpret_cast<TOverlapped*>(lpOverlapped);
			if(TRUE == status) {
				Verify(NULL != pOverlapped->iCompletion);
				pOverlapped->iCompletion->Completed(status, byte_count, pOverlapped);
				DoStats();
				return true;
			} else {
				if(NULL != lpOverlapped) {
					Verify(NULL != pOverlapped->iCompletion);
					pOverlapped->iCompletion->Completed(status, byte_count, pOverlapped);
					return true;
				} else { //timeout
					return false;
				}
			}
		}
	public:
		void Flush() {
			while(true == FlushQueue()) { }
		}
	public:
		virtual ~TIOCP() {
			BOOL check = ::CloseHandle(hIOCP);
			Verify(TRUE == check);
			hIOCP = NULL;
		}
	}; // TIOCP

	////////////////
	class ISession {
	public:
		virtual __int64 get_session_id() = 0;
	public:
		virtual void Send(TMessage &message) = 0;
	public:
		virtual bool IsServerSession() = 0;
	public:
		virtual std::string Description() = 0;
	}; // ISession
	typedef std::shared_ptr<ISession> ISessionPtr;

	/////////////////////////
	class ISessionConnected {
	public:
		virtual void Connected(BOOL status, ISessionPtr session) = 0;
	};
	typedef std::shared_ptr<ISessionConnected> ISessionConnectedPtr;

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
		virtual int Send(LPWSABUF lpBuffers, DWORD dwBufferCount, LPOVERLAPPED lpOverlapped) = 0;
	public:
		virtual int Recv(LPWSABUF lpBuffers, DWORD dwBufferCount, LPOVERLAPPED lpOverlapped) = 0;
	public:
		virtual std::string get_name() = 0;
	public:
		virtual std::string get_peername() = 0;
	public:
		virtual ISessionConnectedPtr &get_session_connected() = 0;
	public:
		virtual ~ISocket() { }
	}; // ISocket

	typedef std::shared_ptr<ISocket> ISocketPtr;

	////////////////////
	class IIOCPEvented {
	public:
		virtual operator HANDLE() = 0;
	public:
		virtual TIOCP &completion_port() = 0;
	}; // IIOCPEvented

	typedef std::shared_ptr<IIOCPEvented> IIOCPEventedPtr;
} /* MurmurBus*/
