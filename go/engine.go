package main

import "C"
import (
	"context"
	"fmt"
	"io"
	"net"
	"os"
)

type Engine struct{}

type Order struct {
	orderId    uint32
	side       inputType
	price      uint32
	count      uint32
	exec_id	   uint32
}

type worker struct {
	inputCh chan work
	toAddHereCh <-chan chan work
	toAddThereCh chan<- chan work
	promisesInCh chan work
	promisesOutCh chan work
}

type work struct {
	order input
	// buffered cap 1: ntg - not processed, input - order to be added, empty input - processed
	toAddCh chan input
	outputCh chan print_input
}

type print_input struct {
	printType string
	request input
	accepted bool
	restingId, newId, execId, price, count uint32
}

var (
	incomingOrders chan work
	printInCh chan work
	printOutCh chan work
)

func init() { 
	incomingOrders = make(chan work)
	printInCh = make(chan work)
	printOutCh = makeUnboundedCh(printInCh)

	go printer()
	go mux() 
}

func mux() {
	orderIdToOrderMap := make(map[uint32]input)
	orderBook := make(map[string]map[inputType]*worker)

	for w := range incomingOrders {
		// Create new instrument both sides if non existent.
		if _, exist := orderBook[w.order.instrument]; !exist {
			orderBook[w.order.instrument] = make(map[inputType]*worker)

			toAddBuyCh := make(chan chan work)
			toAddSellCh := make(chan chan work)
			buyPromisesInCh := make(chan work)
			sellPromisesInCh := make(chan work)

			orderBook[w.order.instrument][inputBuy] = &worker{
				inputCh: make(chan work),
				toAddHereCh: toAddBuyCh,
				toAddThereCh: toAddSellCh,
				promisesInCh: buyPromisesInCh,
				promisesOutCh: makeUnboundedCh(buyPromisesInCh),
			}
			orderBook[w.order.instrument][inputSell] = &worker{
				inputCh: make(chan work),
				toAddHereCh: toAddSellCh,
				toAddThereCh: toAddBuyCh,
				promisesInCh: sellPromisesInCh,
				promisesOutCh: makeUnboundedCh(sellPromisesInCh),
			}

			orderBook[w.order.instrument][inputBuy].start()
			orderBook[w.order.instrument][inputSell].start()
		}

		// Send to correct instrument side.
		var instrument string
		if w.order.orderType == inputCancel {
			order := orderIdToOrderMap[w.order.orderId]
			instrument = order.instrument
			orderBook[order.instrument][order.orderType].inputCh <- w
		} else {
			side := w.order.orderType
			instrument = w.order.instrument
			orderIdToOrderMap[w.order.orderId] = w.order
			if side == inputBuy {
				orderBook[w.order.instrument][inputSell].inputCh <- w
			} else {
				orderBook[w.order.instrument][inputBuy].inputCh <- w
			}
		}

		// To ensure ordering among diff sides same instrument.
		orderBook[instrument][inputSell].promisesInCh <- w // non-blocking
		orderBook[instrument][inputBuy].promisesInCh <- w // non-blocking

		// To ensure ordering among diff instruments.
		printInCh <- w // non-blocking
	}
}

func printer() {
	var counter int64 = 0
	// For every order.
	for w := range printOutCh {
		// For every action done per order.
		for print := range w.outputCh {
			switch print.printType {
			case "add":
				outputOrderAdded(print.request, counter)
			case "delete":
				outputOrderDeleted(print.request, print.accepted, counter)
			case "exec":
				outputOrderExecuted(print.restingId, print.newId, print.execId, print.price, print.count, counter)
			}
			counter += 1
		}
	}
}

// Attempt to make unbounded buffered chan.
func makeUnboundedCh[T any](unbufferedCh chan T) chan T {
	unboundedCh := make(chan T)
	arr := make([]T, 0)
	go func() {
		for {
			if (len(arr) > 0) {
				select {
				case unboundedCh <- arr[0]:
					arr = arr[1:]
				default:
				}
			}
	
			select {
			case item := <-unbufferedCh:
				arr = append(arr, item) 
			default:
			}
		}
	}()
	return unboundedCh
}

func (wkr *worker) start() {
	go func() {
		// Instrument side order book.
		book := make([]*Order, 0)

		empty := input{}

		process_req:
		for {
			select {
			// Sync with wkr.toAddThereCh <- workCh.
			case workCh := <-wkr.toAddHereCh:
				// Should not block as work is sent into workCh before sending workCh into wkr.toAddThereCh.
				wkr := <-workCh
				// Shoud not block as order to be added is sent into wkr.toAddCh before sending wkr into workCh.
				toAdd := <-wkr.toAddCh

				if toAdd != empty {
					wkr.outputCh <- print_input{printType: "add", request: toAdd}
					// Signal end of procesing.
					close(wkr.outputCh)
					book = append(book, &Order{orderId: toAdd.orderId, side: toAdd.orderType, price: toAdd.price, count: toAdd.count, exec_id: 0})
				}

				// Signal done adding or ntg to add in the first place.
				wkr.toAddCh <- input{}
				
			case w := <-wkr.inputCh:
				activeOrder := w.order
				toAddCh := w.toAddCh
				outputCh := w.outputCh

				// Handle cancel orders.
				if activeOrder.orderType == inputCancel {
					cancel_i := -1

					// Find cancel order in order book.
					for i, restingOrder := range book {
						if restingOrder.orderId == activeOrder.orderId {
							// fmt.Fprintf(os.Stderr, "%v found cancel order in order book\n", activeOrder)
							cancel_i = i
							break
						}				
					}

					// Cancel order not found in order book - continue finding cancel order in promises.
					for promiseT := range wkr.promisesOutCh {
						// Check if turn.
						if promiseT.order == activeOrder {
							// If so break to conclude if cancel order is found.
							break
						}

						// If not wait for turn.
						toAdd := <-promiseT.toAddCh

						// If toAdd not empty, it will always be the correct order to add order this side,
						// bcuz the other side blocks until the order is added order this side.
						if toAdd != empty {
							// fmt.Fprintf(os.Stderr, "%v promise not empty\n", w.order)
							
							// Rmb to remove duplicate from toAddHereCh.
							<-wkr.toAddHereCh

							// Add to order book.
							book = append(book, &Order{orderId: toAdd.orderId, side: toAdd.orderType, price: toAdd.price, count: toAdd.count, exec_id: 0})
							promiseT.outputCh <- print_input{printType: "add", request: toAdd}
							// Signal end of processing
							close(promiseT.outputCh)
							
							// Check if new order is cancel order.
							if cancel_i == -1 {
								newOrder_i := len(book) - 1
								newOrder := book[newOrder_i]
								if newOrder.orderId == activeOrder.orderId {
									// fmt.Fprintf(os.Stderr, "%v found cancel order in promises\n", activeOrder)
									cancel_i = newOrder_i
								}
							}
						}

						// Signal done adding or ntg to add in the first place.
						promiseT.toAddCh <- input{}
					}

					// fmt.Fprintf(os.Stderr, "%v done processing promises\n", activeOrder)
					cancelOrderFound := cancel_i != -1
					if (cancelOrderFound) {
						// Delete order.
						book = append(book[:cancel_i], book[cancel_i+1:]...)
						// fmt.Fprintf(os.Stderr, "%v found cancel order in promises book\n", activeOrder)
					}
					// Still no cancel order.
					outputCh <- print_input{printType: "delete", request: activeOrder, accepted: cancelOrderFound}
					// Signal end of processing.
					close(outputCh)
					// Signal ntg to add.
					toAddCh <- input{}
					continue process_req
				}

				// Handle match orders.
				// Flag used when processing promises for partial matching case.
				isMyTurn := false
				process_match:
				for activeOrder.count > 0 {
					// fmt.Fprintf(os.Stderr, "%v matching\n", activeOrder)
					var best_match *Order = nil
					match_i := -1

					// Find match in order book.
					for i, curr := range book {
						// Buy order must be greater than or equal to sell order, and for buy orders the best price is the highest.
						if ((activeOrder.orderType == inputSell && (curr.price >= activeOrder.price) && (best_match == nil || curr.price > best_match.price)) || 
							// Buy order must be greater than or equal to sell order, and for sell orders the best price is the lowest.
							(activeOrder.orderType == inputBuy && (activeOrder.price >= curr.price) && (best_match == nil || curr.price < best_match.price))) {
							best_match = curr
							match_i = i
							// fmt.Fprintf(os.Stderr, "%v found match order order book\n", activeOrder)
						}
					}

					// Match not found in order book - continue finding match in yet to processed promises.
					if (!isMyTurn) {
						for promiseT := range wkr.promisesOutCh {
							// Check if turn.
							if promiseT.order == activeOrder {
								isMyTurn = true
								// If so break to conclude match not found and add to order book.
								break
							}
							
							// If not wait for turn.
							toAdd := <-promiseT.toAddCh
	
							// If toAdd not empty, it will always be the correct order to add order this side,
							// bcuz the other side blocks until the order is added order this side.
							if toAdd != empty {
								// Rmb to remove duplicate from toAddHereCh.
								<-wkr.toAddHereCh

								// Add to order book.
								book = append(book, &Order{orderId: toAdd.orderId, side: toAdd.orderType, price: toAdd.price, count: toAdd.count, exec_id: 0})
								promiseT.outputCh <- print_input{printType: "add", request: toAdd}
								// Signal end of processing.
								close(promiseT.outputCh)
		
								// Check if new order is match.
								newOrder := book[len(book) - 1]
								// Buy order must be greater than or equal to sell order, and for buy orders the best price is the highest.
								if ((activeOrder.orderType == inputSell && (newOrder.price >= activeOrder.price) && (best_match == nil || newOrder.price > best_match.price)) || 
									// Buy order must be greater than or equal to sell order, and for sell orders the best price is the lowest.
									(activeOrder.orderType == inputBuy && (activeOrder.price >= newOrder.price) && (best_match == nil || newOrder.price < best_match.price))) {
									best_match = newOrder
									match_i = len(book) - 1
								}
							}

							// Signal done adding or ntg to add in the first place.
							promiseT.toAddCh <- input{}
						}
					}

					// fmt.Fprintf(os.Stderr, "%v done processing promises\n", activeOrder)

					// Match found order either order book or promises.
					if best_match != nil {
						best_match.exec_id += 1

						// Active order count greater.
						if (activeOrder.count >= best_match.count) {
							// fmt.Fprintf(os.Stderr, "%v active order count greater\n", activeOrder)
							outputCh <- print_input{printType: "exec", request: activeOrder, restingId: best_match.orderId, newId: activeOrder.orderId, execId: best_match.exec_id, price: best_match.price, count: best_match.count}
							activeOrder.count -= best_match.count
							// Delete order.
							book = append(book[:match_i], book[match_i+1:]...)
							// Partial match - continue matching active order.
							continue process_match
						}
						
						// fmt.Fprintf(os.Stderr, "%v resting order count greater\n", activeOrder)
						// Resting order count greater.
						outputCh <- print_input{printType: "exec", request: activeOrder, restingId: best_match.orderId, newId: activeOrder.orderId, execId: best_match.exec_id, price: best_match.price, count: activeOrder.count}
						// Signal end of processing.
						close(outputCh)
						best_match.count -= activeOrder.count;
						activeOrder.count = 0;
						// Signal ntg to add.
						toAddCh <- input{}
						continue process_req
					}
					
					// Still no match - add to order book.
					// fmt.Fprintf(os.Stderr, "%v no match found\n", activeOrder)
					toAddCh <- activeOrder
					workCh := make(chan work, 1)
					workCh <- w
					// Sync with workCh := <-wkr.toAddHereCh.
					wkr.toAddThereCh <- workCh
					// Signaling of done adding and end of processing to be handled aft order added.
					continue process_req
				}

				// Signal ntg to add.
				toAddCh <- input{}
				// Signal end of processing.
				close(outputCh)

			default:
			}
		}
	}()
}

func (e *Engine) accept(ctx context.Context, conn net.Conn) {
	go func() {
		<-ctx.Done() 
		conn.Close()
	}()
	go handleConn(conn)
}

func handleConn(conn net.Conn) {
	defer conn.Close()

	clientOrderIds := make(map[uint32]struct{})

	for {
		in, err := readInput(conn)
		if err != nil {
			if err != io.EOF {
				_, _ = fmt.Fprintf(os.Stderr, "Error reading input: %v\n", err)
			}
			return
		}

		// Check if cancel order sent by client.
		switch in.orderType {
		case inputCancel:
			if _, found := clientOrderIds[in.orderId]; !found {
				fmt.Fprintf(os.Stderr, "Order ID %v not sent by this client\n", in.orderId)
			}
		default:
			clientOrderIds[in.orderId] = struct{}{}
		}

		// Send input to mux to be dispatch to corresponding instrument side worker.
		incomingOrders <- work{order: in, toAddCh: make(chan input, 1), outputCh: make(chan print_input)}
	}
}
