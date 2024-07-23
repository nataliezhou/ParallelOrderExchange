#include <iostream>
#include <thread>

#include "engine.hpp"

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;

				// Check if Order exists.
				auto order_info = orders.find(input.order_id);

				// Order does not exist.
				if (!order_info.value)
				{
					// Nothing to delete, deletion rejected.
					Output::OrderDeleted(input.order_id, false, order_info.timestamp);
					continue;
				}

				// Order exists.
				TSLinkedList::DeleteInfo deleteInfo;
				// Client can only cancel its Orders.
				if (order_info.value->client_id == std::this_thread::get_id())
				{
					// Attempt to delete order.
					deleteInfo = order_book.find(order_info.value->instrument).value->erase(input.order_id);

					// Order may be deleted by other threads and deletion rejected,
					// otherwise deletion accepted.
					if (deleteInfo.is_deleted)
					{
						orders.erase(input.order_id);
					}
				}
				Output::OrderDeleted(input.order_id, deleteInfo.is_deleted, deleteInfo.timestamp);
				break;
			}

			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;
					
				// Order size must be greater than 0.
				if (input.count == 0) continue;

				// Match Order until no more matches possible.
				TSLinkedList::MatchInfo matchInfo;
				bool matchSuccess;
				while (true)
				{
					// Match Order.
					matchInfo = order_book.get(input.instrument)->match(&input, &orders);
					matchSuccess = !matchInfo.is_default;

					// Check if match.
					if (matchSuccess)
					{
						Output::OrderExecuted(
							matchInfo.resting_id,
							matchInfo.new_id,
							matchInfo.execution_id,
							matchInfo.price,
							matchInfo.count,
							matchInfo.timestamp);
					} else {
						// If no match, Order is only added when count > 0
						if (input.count > 0) {
							Output::OrderAdded(
								input.order_id,
								input.instrument,
								input.price,
								input.count,
								input.type == input_sell,
								matchInfo.timestamp);
						}
						break;
					}
				}
				
				break;
			}
		}

	}
}
