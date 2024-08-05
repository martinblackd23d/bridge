# Overview

Types of vehicles:

Car - weight: 200, probability: 0.5

Van - weight: 300, probability: 0.5

Bridge weight capacity: 1200

1. Vehicles cannot change lanes while on the bridge

2. Traffic can only flow in one direction in a given lane at a given time

3. The total weight of vehicles on the bridge at any given time may not exceed the max weight capacity of the bridge

3. If there's traffic (or waiting cars) in both directions, north and southbound lanes are predetermined (ie right-hand traffic).

4. If there's traffic (or waiting cars) in only one direction, traffic is evenly split between the two lanes, skewed towards the preferred lane.

5. Vehicles are allowed to pass through in a first in - first out queue, as soon as the weight allows, regardless of direction. This is to avoid scenarios where only cars pass through and there's never enough margin in the weight restriction for a van to pass through.

- Note: dynamic lane allocation is really easy to implement. If cars switch to the most congested lane instead of the preferred lane or least congested lane, the other lane will eventually empty out and be open to opposing traffic. This is slightly more efficient, but makes no sense with regards to real traffic flows.

# Data structures

## Logging
- starttime: timespec - start time of the program
- printCalled: int - whether print has been called recently

## Vehicle
- id: int
- type: int - 0: car, 1: van
- weight: int
- dir: int - -1: southbound, 1: northbound
- lane: int - -1: southbound lane, 1: northbound lane
- bridge: Bridge - reference to the bridge the vehicle is on
- cond: pthread_cond_t *- condition variable for the specific vehicle's thread

## Queue
linked list of vehicles
- vehicles: Vehicle * - vehicle of this element
- next: Queue * - next element in the queue

## VehicleList
linked list of vehicles on the bridge
- vehicle: Vehicle * - vehicle of this element
- next: VehicleList * - next element in the list

## Bridge
- open: int - 1: bridge is open, 0: bridge is closed
- lock: pthread_mutex_t - lock for the bridge
- waiting: pthread_cond_t - condition variable for updating the queue
- laneNorthbound: int - number of vehicles in the northbound preferred lane, negative if southbound flow, positive if northbound flow
- laneSouthbound: int - number of vehicles in the southbound preferred lane, negative if southbound flow, positive if northbound flow
- weight: int - weight of vehicles on the bridge
- southboundWaiting: int - number of vehicles waiting in the southbound direction
- northboundWaiting: int - number of vehicles waiting in the northbound direction
- queue: Queue * - queue of vehicles waiting to enter the bridge
- vehicles: VehicleList * - list of vehicles on the bridge

## VehicleArgs
- id: int - id of the vehicle
- type: int - type of the vehicle
- dir: int - direction of the vehicle
- bridge: Bridge * - reference to the bridge

## Group
- count: int - number of vehicles in the group
- prob: double - probability of northbound vehicles
- delay: int - delay between groups in seconds

# Pseudocode
```
getTime():
	return time since start of program

addQueue(vehicle):
	add vehicle to the end of the queue

	if queue

	if vehicle is northbound:
		increment northboundWaiting
	else:
		increment southboundWaiting

	signal queue update

removeQueue(vehicle):
	remove first vehicle from the queue

	if vehicle is northbound:
		decrement northboundWaiting
	else:
		decrement southboundWaiting

	signal next vehicle in queue

removeFromBridge(vehicle):
	remove vehicle from the list

	bridge weight -= vehicle weight
	if vehicle is in northbound lane:
		decrement laneNorthbound based on direction
	else:
		decrement laneSouthbound based on direction

	signal next vehicle in list

addToBridge(vehicle):
	add vehicle to the end of the list

	bridge weight += vehicle weight
	if vehicle is in northbound lane:
		increment laneNorthbound based on direction
	else:
		increment laneSouthbound based on direction

printBridgeThreaded():
	sleep for 0.5 seconds
	lock bridge
	print bridge northbound lane
	print bridge southbound lane
	print northbound waiting
	print southbound waiting
	reset printCalled
	unlock bridge

printBridge():
	if printCalled:
		return
	set printCalled
	call printBridgeThreaded in a new thread

arrive(vehicle):
	// already locked
	print vehicle arrival
	addQueue(vehicle)
	while bridge is closed OR vehicle is not first in queue OR vehicle weight + bridge weight > max weight:
		wait for signal to vehicle

	if vehicle is southbound:
		if laneNorthbound is empty or has southboundflow
				AND no vehicles waiting northbound
				AND laneNorthbound has less vehicles than laneSouthbound:
			vehicle lane = northbound
		else:
			vehicle lane = southbound
	else:
		if laneSouthbound is empty or has northboundflow
				AND no vehicles waiting southbound
				AND laneSouthbound has less vehicles than laneNorthbound:
			vehicle lane = southbound
		else:
			vehicle lane = northbound

cross(vehicle):
	// already locked
	print vehicle crossing

	removeQueue(vehicle)
	addToBridge(vehicle)
	printBridge()

	release lock
	sleep for 3 seconds
	lock bridge

leave(vehicle):
	// already locked
	print vehicle leaving

	removeFromBridge(vehicle)

vehicle_routine(VehicleArgs):
	create vehicle from arguments

	lock bridge
	arrive(vehicle)
	cross(vehicle) // releases and reaquires lock
	leave(vehicle)
	unlock bridge

	destroy vehicle

main(argc, argv):
	check invalid arguments
	if argc == 1:
		prompt user for input
	if argc == 2:
		read file
	if argc > 2:
		read command line arguments

	for i in group count:
		read input source
		check for invalid input
	
	set start time
	create bridge

	for i in group count:
		lock bridge
		close bridge
		save bridge state

		for j in group vehicle count:
			generate vehicle
			create thread with vehicle_routine

		while NOT all created vehicles have arrived and queued:
			wait for signal to queue

		open bridge
		signal first vehicle in queue
		unlock bridge

		sleep for group delay

	for thread in threads:
		join thread
```

