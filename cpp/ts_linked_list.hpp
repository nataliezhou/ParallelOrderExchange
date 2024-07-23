#pragma once

#include <memory>
#include <mutex>
#include <thread>

#include "io.hpp"
#include "ts_map.cpp"

class TSLinkedList
{
private:
    struct Order
    {
        uint32_t order_id;
        std::thread::id client_id;
        uint32_t price;
        uint32_t count;
        CommandType type;
        uint32_t execution_id;

        Order() {};
        Order(uint32_t order_id, std::thread::id client_id, uint32_t price,
              uint32_t count, CommandType type)
            : order_id{order_id}, client_id{client_id}, price{price},
              count{count}, type{type}, execution_id{0} {};
    };

    struct Node
    {
        Order order;
        std::mutex mut;
        Node* next;

        Node() : next{nullptr} {};
    };

    Node* front;

public:
    TSLinkedList();
    ~TSLinkedList();
    TSLinkedList(const TSLinkedList& other) = delete;
    TSLinkedList& operator=(const TSLinkedList& other) = delete;

    struct MatchInfo
    {
        uint32_t resting_id;
        uint32_t new_id;
        uint32_t execution_id;
        uint32_t price;
        uint32_t count;
        intmax_t timestamp = getCurrentTimestamp();
        bool is_default;

        MatchInfo() : is_default{true} {};
        MatchInfo(uint32_t resting_id, uint32_t new_id, uint32_t execution_id,
                  uint32_t price, uint32_t count)
            : resting_id{resting_id}, new_id{new_id}, execution_id{execution_id},
              price{price}, count{count}, is_default{false} {};
    };
    struct OrderInfo
    {
        std::thread::id client_id;
        std::string instrument;
        intmax_t timestamp = getCurrentTimestamp();

        OrderInfo(){};
        OrderInfo(std::thread::id client_id, std::string instrument)
            : client_id{client_id}, instrument{instrument} {};
    };
    // Return filled MatchInfo upon successful matching,
    // otherwise return defaultly constructed MatchInfo,
    // i.e., is_default = true.
    // Order will be updated upon successful partial/full match,
    // and might be added into order_book and orders.
    MatchInfo match(ClientCommand* input,
        TSMap<uint32_t, OrderInfo>* orders);

    struct DeleteInfo
    {
        intmax_t timestamp = getCurrentTimestamp();
        bool is_deleted;

        DeleteInfo(): is_deleted{false} {};
        DeleteInfo(bool is_deleted) : is_deleted{is_deleted} {};
    };
    // Return true upon successful deletion, otherwise false.
    DeleteInfo erase(uint32_t order_id);
};