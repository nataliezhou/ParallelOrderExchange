// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <utility>

#include "io.hpp"
#include "ts_linked_list.hpp"

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	typedef std::string Instrument;
	TSMap<Instrument, TSLinkedList> order_book{};

	// Unique across all orders, can be used as key.
	typedef uint32_t OrderId;
	TSMap<OrderId, TSLinkedList::OrderInfo> orders{};
	
	void connection_thread(ClientConnection conn);
};

#endif
