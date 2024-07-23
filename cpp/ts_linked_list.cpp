#include "ts_linked_list.hpp"

TSLinkedList::TSLinkedList()
    : front{new Node{}}
{
    front->next = new Node{};
}

TSLinkedList::~TSLinkedList()
{
    while (front != nullptr)
    {
        Node* next = front->next;
        delete front;
        front = next;
    }
}

TSLinkedList::MatchInfo TSLinkedList::match(ClientCommand *input,
    TSMap<uint32_t, OrderInfo> *orders)
{
    // If count is 0, nothing to match.
    if (input->count == 0) return MatchInfo{};

    // Hold best_prev to facilitate deletion.
    Node* best_prev = nullptr;
    std::unique_lock<std::mutex> best_prev_lk;

    // Find best match by traversing entire LinkedList.
    std::unique_lock<std::mutex> prev_lk{front->mut};
    Node* prev = front; // front is dummy
    Node* curr = front->next;
    while (true)
    {
        std::unique_lock<std::mutex> curr_lk{curr->mut};

        // While loop exit point:
        // curr->next == nullptr indicates curr is the dummy back node.
        if (curr->next == nullptr)
        {
            // Need to hold back->mut until match processing
            // to prevent add before match.
            // Now prev_lk holds back->mut - variable reuse not ideal though.
            prev_lk = std::move(curr_lk);
            prev = curr;
            break;
        }

        if (input->type == input_buy && curr->order.type == input_sell)
        {
            // Buy order must be greater than or equal to sell order.
            // As input is buy, curr must be sell.
            if (input->price >= curr->order.price &&
                // For sell orders, the best price is the lowest.
                (!best_prev || curr->order.price < best_prev->next->order.price))
            {
                best_prev = prev;
                // Hold lock to best_prev until next best is found.
                best_prev_lk = std::move(prev_lk);
            }
            else
            {
                prev_lk.unlock();
            }
        }
        else if (input->type == input_sell && curr->order.type == input_buy)
        {
            // Buy order must be greater than or equal to sell order.
            // As input is sell, curr must be buy.
            if (curr->order.price >= input->price &&
                // For buy orders, the best price is the highest.
                (!best_prev || curr->order.price > best_prev->next->order.price))
            {
                best_prev = prev;
                // Hold lock to best_prev until next best is found.
                best_prev_lk = std::move(prev_lk);
            }
            else
            {
                prev_lk.unlock();
            }
        } else {
            prev_lk.unlock();
        }

        // Update prev, curr and prev_lk to continue traversing.
        prev = curr;
        curr = curr->next;
        prev_lk = std::move(curr_lk);
    }

    // Check if match found.
    if (best_prev)
    {
        // If match found, update count and return MatchInfo.
        Node* best = best_prev->next;
        if (input->count >= best->order.count)
        {
            // Construct MatchInfo before deleting match.
            auto matchInfo = MatchInfo{
                best->order.order_id,
                input->order_id,
                best->order.execution_id + 1,
                best->order.price,
                best->order.count,
            };

            // Decrement input count.
            input->count -= best->order.count;

            // Delete match.
            Node* temp = best->next;
            delete best;
            best_prev->next = temp;

            return matchInfo;
        }
        else /*if (input->count < best_prev->next->order.count)*/
        {
            // Decrement match count.
            best->order.count -= input->count;
            best->order.execution_id += 1;

            // Construct MatchInfo before updating input count.
            auto matchInfo = MatchInfo{
                best->order.order_id,
                input->order_id,
                best->order.execution_id,
                best->order.price,
                input->count,
            };

            // Update input count.
            input->count = 0;

            return matchInfo;
        }
    }

    // If match not found, add Order to order_book and orders.
    // Note that input->count > 0, else match() would alr returned at the start.
    Order new_order = Order{
        input->order_id,
        std::this_thread::get_id(),
        input->price,
        input->count,
        input->type,
    };
    prev->order = new_order;
    // New back dummy node.
    prev->next = new Node{};

    orders->emplace_back(input->order_id, new OrderInfo {
        std::this_thread::get_id(),
        input->instrument,
    });

    return MatchInfo{};
}

TSLinkedList::DeleteInfo TSLinkedList::erase(uint32_t order_id)
{
    std::unique_lock<std::mutex> prev_lk{front->mut};
    Node* prev = front; // front is dummy
    Node* curr = front->next;

    // Find Order to delete by traversing entire LinkedList.
    while (true)
    {
        std::unique_lock<std::mutex> curr_lk{curr->mut};

        // While loop exit point:
        // curr->next == nullptr indicates curr is the dummy back node.
        if (curr->next == nullptr) return DeleteInfo{false};

        if(curr->order.order_id == order_id)
        {
            Node* temp = curr->next;
            curr_lk.unlock();
            delete curr;
            prev->next = temp;
            return DeleteInfo{true};
        }

        prev_lk.unlock();

        // Update prev, curr and prev_lk to continue traversing.
        prev = curr;
        curr = curr->next;
        prev_lk = std::move(curr_lk);
    }
}
