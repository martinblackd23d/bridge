#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#define MAX_WEIGHT 1200
#define CAR_WEIGHT 200
#define VAN_WEIGHT 300
#define CAR_PROB 0.5

// helpers for logging
struct timespec starttime = {0, 0};
double getTime() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - starttime.tv_sec) + (now.tv_nsec - starttime.tv_nsec) / 1e9;
}
int printCalled = 0;

typedef struct Vehicle Vehicle;
typedef struct Queue Queue;
typedef struct VehicleList VehicleList;
typedef struct Bridge Bridge;
typedef struct VehicleArgs VehicleArgs;

void addQueue(Vehicle *vehicle);
void removeQueue(Vehicle *vehicle);
void addToBridge(Vehicle *vehicle);
void removeFromBridge(Vehicle *vehicle);
void *printBridgeThreaded(void *bridge);
void printBridge(Bridge *bridge);
void arrive(Vehicle *vehicle);
void cross(Vehicle *vehicle);
void leave(Vehicle *vehicle);
void *vehicle_routine(void *arg);
int main(int argc, char *argv[]);

// Vehicle
struct Vehicle{
	int id;
	int type;
	int weight;
	int dir; // -1 southbound, 1 northbound
	int lane;
	Bridge *bridge;
	pthread_cond_t *cond; // specific to the vehicle
};

// Linked List Queue for vehicles waiting to cross the bridge
struct Queue {
	Vehicle *vehicle;
	Queue *next;
};

// Linked List of vehicles on the bridge
struct VehicleList {
	Vehicle *vehicle;
	VehicleList *next;
};

// Bridge
struct Bridge {
	int open; // 0 - closed, 1 - open
	pthread_mutex_t lock;
	pthread_cond_t waiting; // condition variable for updating the queue

	// negative values indicate southbound traffic flow, positive values indicate northbound traffic flow
	int laneSouthbound; // number of vehicles in south preferred lane
	int laneNorthbound; // number of vehicles in north preferred lane

	int weight; // total weight of vehicles on the bridge
	int southboundWaiting; // number of southbound vehicles waiting to cross the bridge
	int northboundWaiting; // number of northbound vehicles waiting to cross the bridge
	Queue *queue;
	VehicleList *vehicles;
};

// Adds a vehicle to the end of the queue
void addQueue(Vehicle *vehicle) {\
	Bridge *bridge = vehicle->bridge;
	Queue *queue = bridge->queue;
	Queue *newQueue = (Queue*)malloc(sizeof(Queue));

	// create new queue element
	newQueue->vehicle = vehicle;
	newQueue->next = NULL;

	// add to the end of the queue
	if (queue == NULL) {
		bridge->queue = newQueue;
	} else {
		while (queue->next != NULL) {
			queue = queue->next;
		}
		queue->next = newQueue;
	}

	// increment the number of vehicles waiting in the correct direction
	if (vehicle->dir == -1) {
		bridge->southboundWaiting++;
	} else {
		bridge->northboundWaiting++;
	}

	// signal an update to queue
	pthread_cond_signal(&bridge->waiting);
}

// Removes first vehicle from the queue
void removeQueue(Vehicle *vehicle) {
	Bridge *bridge = vehicle->bridge;
	Queue *queue = bridge->queue;
	bridge->queue = queue->next;
	free(queue);

	// decrement the number of vehicles waiting in the correct direction
	if (vehicle->dir == -1) {
		bridge->southboundWaiting--;
	} else {
		bridge->northboundWaiting--;
	}

	// signal next vehicle in queue
	if (bridge->queue != NULL)
		pthread_cond_signal(bridge->queue->vehicle->cond);
}

// Removes vehicle from the bridge
void removeFromBridge(Vehicle *vehicle) {
	Bridge *bridge = vehicle->bridge;
	VehicleList *list = bridge->vehicles;
	VehicleList *prev = NULL;

	// find the vehicle in the list
	while (list != NULL) {
		if (list->vehicle->id == vehicle->id) {
			if (prev == NULL) {
				bridge->vehicles = list->next;
			} else {
				prev->next = list->next;
			}
			free(list);
			break;
		}
		prev = list;
		list = list->next;
	}

	// update the bridge weight and lane counts
	bridge->weight -= vehicle->weight;
	if (vehicle->lane == 1) {
		bridge->laneNorthbound -= vehicle->dir;
	} else {
		bridge->laneSouthbound -= vehicle->dir;
	}

	// signal next vehicle in queue
	if (bridge->queue != NULL)
		pthread_cond_signal(bridge->queue->vehicle->cond);
}

// Adds vehicle to bridge
void addToBridge(Vehicle *vehicle) {
	Bridge *bridge = vehicle->bridge;
	VehicleList *list = bridge->vehicles;
	VehicleList *newList = (VehicleList *)malloc(sizeof(VehicleList));

	// create new list element
	newList->vehicle = vehicle;
	newList->next = NULL;

	// add to the end of the list
	if (list == NULL) {
		bridge->vehicles = newList;
	} else {
		while (list->next != NULL) {
			list = list->next;
		}
		list->next = newList;
	}
	
	// update the bridge weight and lane counts
	bridge->weight += vehicle->weight;
	if (vehicle->lane == 1) {
		bridge->laneNorthbound += vehicle->dir;
	} else {
		bridge->laneSouthbound += vehicle->dir;
	}
}

// Prints the current state of the bridge in a new thread
void *printBridgeThreaded(void *vbridge) {
	// sleep for 0.5 seconds in case a large number of updates happen at once
	usleep(500000);
	Bridge *bridge = (Bridge *)vbridge;
	pthread_mutex_lock(&bridge->lock);

	// print northbound lane
	printf("\n%.2lf Bridge status: Lane 1 (north default) - flow: %s: [", getTime(), bridge->laneNorthbound >= 0 ? "northbound" : "southbound");
	for (VehicleList *v = bridge->vehicles; v != NULL; v = v->next) {
		if (v->vehicle->lane == 1)
			printf("%s #%d (%s) ",
				v->vehicle->type == 0 ? "Car" : "Van",
				v->vehicle->id,
				v->vehicle->dir == -1 ? "Southbound" : "Northbound");
	}
	// print southbound lane
	printf("] Lane -1 (south default) - flow: %s: [", bridge->laneSouthbound <= 0 ? "southbound" : "northbound");
	for (VehicleList *v = bridge->vehicles; v != NULL; v = v->next) {
		if (v->vehicle->lane == -1)
			printf("%s #%d (%s) ",
				v->vehicle->type == 0 ? "Car" : "Van",
				v->vehicle->id,
				v->vehicle->dir == -1 ? "Southbound" : "Northbound");
	}
	printf("]\n");
	// print waiting northbound
	printf("\tWaiting Northbound: [");
	for (Queue *q = bridge->queue; q != NULL; q = q->next) {
		if (q->vehicle->dir == 1)
			printf("%s #%d (%s) ",
				q->vehicle->type == 0 ? "Car" : "Van",
				q->vehicle->id,
				q->vehicle->dir == -1 ? "Southbound" : "Northbound");
	}
	// print waiting southbound
	printf("] Waiting Southbound: [");
	for (Queue *q = bridge->queue; q != NULL; q = q->next) {
		if (q->vehicle->dir == -1)
			printf("%s #%d (%s) ",
				q->vehicle->type == 0 ? "Car" : "Van",
				q->vehicle->id,
				q->vehicle->dir == -1 ? "Southbound" : "Northbound");
	}
	printf("]\n\n");
	// reset printCalled flag
	printCalled = 0;
	pthread_mutex_unlock(&bridge->lock);
	return NULL;
}

// calls printBridgeThreaded in a new thread
void printBridge(Bridge *bridge) {
	// if print was scheduled recently, do not schedule again
	if (printCalled != 0) {
		return;
	}
	printCalled = 1;
	pthread_t thread;
	pthread_create(&thread, NULL, printBridgeThreaded, bridge);
}

// Vehicle arrival
void arrive(Vehicle *vehicle) {
	// already locked
	Bridge *bridge = vehicle->bridge;

	// print arrival
	printf("%.2lf %s #%d (%s) arrived.\n",
		getTime(),
		vehicle->type == 0 ? "Car" : "Van",
		vehicle->id,
		vehicle->dir == -1 ? "Southbound" : "Northbound");

	// add vehicle to queue
	addQueue(vehicle);
	
	// wait for
	//		the vehicle to be at the front of the queue
	//		the bridge to be open
	//		the bridge to not be overloaded
	while (vehicle->weight + bridge->weight > MAX_WEIGHT || bridge->open == 0 || bridge->queue == NULL || bridge->queue->vehicle->id != vehicle->id) {
		pthread_cond_wait(vehicle->cond, &bridge->lock);
	}

	// determine vehicle lane
	// rules:
	// choose the preferred lane for the vehicle's direction
	// unless:
	//		there's no traffic in the opposite direction
	//		there's no waiting vehicles in the opposite direction
	//		and the other lane is less congested
	if (vehicle->dir == -1) {
		if (bridge->laneNorthbound <= 0 && bridge->laneNorthbound > bridge->laneSouthbound && bridge->northboundWaiting == 0) {
			vehicle->lane = 1;
		} else {
			vehicle->lane = -1;
		}
	} else {
		if (bridge->laneSouthbound >= 0 && bridge->laneSouthbound < bridge->laneNorthbound && bridge->southboundWaiting == 0) {
			vehicle->lane = -1;
		} else {
			vehicle->lane = 1;
		}
	}

}

// Vehicle crossing
void cross(Vehicle *vehicle) {
	// already locked
	Bridge *bridge = vehicle->bridge;
	// print crossing
	printf("%.2lf %s #%d (%s) is now crossing the bridge in %s lane.\n",
		getTime(),
		vehicle->type == 0 ? "Car" : "Van",
		vehicle->id,
		vehicle->dir == -1 ? "Southbound" : "Northbound",
		vehicle->lane == 1 ? "northbound" : "southbound");
	// update queue and bridge, print bridge status
	removeQueue(vehicle);
	addToBridge(vehicle);
	printBridge(bridge);
	// release lock and sleep for 3 seconds
	pthread_mutex_unlock(&bridge->lock);
	sleep(3);
	pthread_mutex_lock(&bridge->lock);
}

// Vehicle leaving
void leave(Vehicle *vehicle) {
	// already locked
	// print leaving
	printf("%.2lf %s #%d (%s) has exited the bridge.\n",
		getTime(),
		vehicle->type == 0 ? "Car" : "Van",
		vehicle->id,
		vehicle->dir == -1 ? "Southbound" : "Northbound");
	// update bridge
	removeFromBridge(vehicle);
}

// Vehicle thread routine
struct VehicleArgs {
	int id;
	int type;
	int dir;
	Bridge *bridge;
};
void *vehicle_routine(void *arg) {
	// extract arguments
	struct VehicleArgs *args = (struct VehicleArgs *)arg;
	Vehicle *vehicle = (Vehicle *)malloc(sizeof(Vehicle));
	vehicle->id = args->id;
	vehicle->dir = args->dir;
	vehicle->type = args->type;
	vehicle->weight = args->type == 0 ? CAR_WEIGHT : VAN_WEIGHT;
	vehicle->bridge = args->bridge;
	vehicle->cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(vehicle->cond, NULL);

	// main routine
	pthread_mutex_lock(&vehicle->bridge->lock);
	arrive(vehicle);
	cross(vehicle); // releases and reaquires lock
	leave(vehicle);
	pthread_mutex_unlock(&vehicle->bridge->lock);

	// cleanup
	pthread_cond_destroy(vehicle->cond);
	free(vehicle);
	free(args);
	return NULL;
}

// Main function
int main(int argc, char *argv[]) {
	// check argument count and input source
	int groups = 0;
	FILE *file = NULL;
	if ((argc - 1) % 3 != 0 && argc > 2) {
		printf("Error. Usage: \n./bridge\n./bridge <vehicle count> <probability of northbound> <delay> { <vehicle count> <probability of northbound> <delay> ... }\n./bridge schedule.txt\n");
		return 1;
	}
	if (argc == 1) {
		printf("Enter the number of groups: ");
		if (scanf("%d", &groups) != 1) {
			printf("Error. Invalid input.\n");
			return 1;
		}
	}
	if (argc == 2) {
		file = fopen(argv[1], "r");
		if (file == NULL) {
			printf("Error. File not found.\n");
			return 1;
		}
		if (fscanf(file, "%d", &groups) == 0) {
			printf("Error. Invalid input.\n");
			return 1;
		}
	}
	if (argc > 2) {
		groups = (argc - 1) / 3;
	}

	// read data
	typedef struct {
		int count;
		double P_N;
		int delay;
	} Group;

	Group *schedule = malloc(sizeof(Group) * groups * 3);

	for (int i = 0; i < groups; i++) {
		if (argc == 1) {
			// read from stdin
			printf("Enter the number of vehicles in group %d: ", i + 1);
			if (scanf("%d", &schedule[i].count) != 1) {
				printf("Error. Invalid input.\n");
				return 1;
			}
			printf("Enter the percentage of northbound in group %d: ", i + 1);
			if (scanf("%lf", &schedule[i].P_N) != 1) {
				printf("Error. Invalid input.\n");
				return 1;
			}
			printf("Enter the delay in seconds for group %d: ", i + 1);
			if (scanf("%d", &schedule[i].delay) != 1) {
				printf("Error. Invalid input.\n");
				return 1;
			}
		} else if (argc == 2) {
			// read from file
			if (fscanf(file, "%d %lf %d", &schedule[i].count, &schedule[i].P_N, &schedule[i].delay) != 3) {
				printf("Error. Invalid input.\n");
				fclose(file);
				return 1;
			}
		} else {
			// read from arguments
			char *endptr;
			schedule[i].count = strtol(argv[i * 3 + 1], &endptr, 10);
			if (*endptr != '\0') {
				printf("Error. Invalid input.\n");
				return 1;
			}
			schedule[i].P_N = strtod(argv[i * 3 + 2], &endptr);
			if (*endptr != '\0') {
				printf("Error. Invalid input.\n");
				return 1;
			}
			schedule[i].delay = strtol(argv[i * 3 + 3], &endptr, 10);
			if (*endptr != '\0') {
				printf("Error. Invalid input.\n");
				return 1;
			}
		}
		// validate input
		if (schedule[i].P_N < 0 || schedule[i].P_N > 1 || schedule[i].delay < 0 || schedule[i].count <= 0) {
			printf("Error. Invalid input.\n");
			return 1;
		}
	}
	// close file
	if (argc == 2)
		fclose(file);

	// initialize bridge and random number generator
	int seed = time(NULL);
	srand(seed);
	struct Bridge bridge = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0, 0, 0, NULL, NULL};

	pthread_t **threads = malloc(sizeof(pthread_t*) * groups);

	// start time
	clock_gettime(CLOCK_MONOTONIC, &starttime);

	// create vehicles
	int id = 0;
	for (int i = 0; i < groups; i++) {
		threads[i] = malloc(sizeof(pthread_t) * schedule[i].count);

		// close bridge and save state
		// ensures all vehicles arrive at the same time
		pthread_mutex_lock(&bridge.lock);
		bridge.open = 0;
		int alreadyWaiting = bridge.northboundWaiting + bridge.southboundWaiting;

		for (int j = 0; j < schedule[i].count; j++) {
			// generate vehicle
			int dir = rand() < schedule[i].P_N * INT_MAX ? 1 : -1;
			int type = rand() < CAR_PROB * INT_MAX ? 0 : 1;
			struct VehicleArgs *args = (struct VehicleArgs *)malloc(sizeof(struct VehicleArgs));

			// create vehicle thread
			args->id = id++;
			args->type = type;
			args->dir = dir;
			args->bridge = &bridge;
			pthread_create(&threads[i][j], NULL, vehicle_routine, args);
		}

		// wait for all vehicles to be created and waiting
		while (bridge.northboundWaiting + bridge.southboundWaiting < schedule[i].count + alreadyWaiting) {
			pthread_cond_wait(&bridge.waiting, &bridge.lock);
		}

		// open bridge and signal first vehicle
		bridge.open = 1;
		pthread_cond_signal(bridge.queue->vehicle->cond);

		pthread_mutex_unlock(&bridge.lock);
		// sleep until next group
		sleep(schedule[i].delay);
	}

	// wait for all threads to finish
	for (int i = 0; i < groups; i++) {
		for (int j = 0; j < schedule[i].count; j++) {
			pthread_join(threads[i][j], NULL);
		}
	}

	return 0;
}