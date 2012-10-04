// Rio.lib.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "../include/RIO.lib.h"
#include <iostream>

using namespace MurmurBus;
using namespace MurmurBus::IOCP;

void test_TUUID() {
	TUUID test;
	std::cout << (std::string)test << std::endl << std::endl;
}

void test_TMessage() {
	TMessage msg;
	msg["arg1"] = "val1";
	msg["arg2"] = "val2";
	msg.Dump(std::cout);

	std::string buffer;
	msg.Append(buffer);

	TMessage msg2;
	size_t next_offset = 0;
	Verify(true == msg2.Read(buffer, 0, &next_offset));
	msg2.Dump(std::cout);
}

int _tmain(int argc, _TCHAR* argv[])
{
	IEventPtr events = IEventPtr(new TEvent());
	IIOCPEventedPtr iocp_evented = IIOCPEventedPtr(new TIOCPEvented(events));
	TEchoTest echo_test(iocp_evented, "127.0.0.1", 333, 128);
	TPubSub publisher(iocp_evented);

	for(;;) {
		events->WaitOne();
		iocp_evented->completion_port().Flush();
	}

	return 0;
}
