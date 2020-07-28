#include "rtma_client.h"
#include <vector>
#include <thread>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MT_TEST_MSG 1234
#define MT_PUBLISHER_READY 5677
#define MT_PUBLISHER_DONE 5678
#define MT_SUBSCRIBER_READY 5679
#define MT_SUBSCRIBER_DONE 5680

typedef struct {
	char data[];
}MDF_TEST_MSG;


int subscriber_loop(int id, char* server, int port, int num_msgs, int msg_size) {
	Client* c = rtma_create_client(0, 0);
	rtma_client_connect(c, server, port);
	rtma_client_subscribe(c, MT_EXIT);
	rtma_client_subscribe(c, MT_TEST_MSG);
	rtma_client_send_module_ready(c);

	int msg_rcvd = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
	std::chrono::time_point<std::chrono::high_resolution_clock> end;

	int nbytes = rtma_client_send_signal(c, MT_SUBSCRIBER_READY);

	Message msg;
	while (msg_rcvd < num_msgs) {
		if (rtma_client_read_message(c, &msg, BLOCKING)) {
			switch (MSG_TYPE(msg)) {
			case MT_TEST_MSG:
				if (msg_rcvd == 0)
					start = std::chrono::high_resolution_clock::now();
				end = std::chrono::high_resolution_clock::now();
				msg_rcvd++;
				break;
			case MT_EXIT:
				goto quit;
			}
		}
	}

	rtma_client_send_signal(c, MT_SUBSCRIBER_DONE);

quit:
	std::chrono::duration<double> dur = end - start;
	double data_transfer = (double(msg_rcvd) - 1.0) * double(msg_size + sizeof(RTMA_MSG_HEADER)) / double(1024) / double(1024) / dur.count();

	rtma_client_disconnect(c);
	rtma_destroy_client(&c);

	if (msg_rcvd == num_msgs) {
		printf("Subscriber[%d] -> %d messages | %d messages/sec | %0.1lf MB/sec | %0.6lf sec\n",
			id,
			msg_rcvd,
			int((double(msg_rcvd) - 1.0) / dur.count()),
			data_transfer,
			dur.count());
		}
	else {
		printf("Subscriber[%d] -> %d messages (%0d%%) | %d messages/sec | %0.1lf MB/sec | %0.6lf sec\n",
			id,
			msg_rcvd,
			(int)((double(msg_rcvd) - 1.0)/ double(num_msgs) * 100.0),
			int((double(msg_rcvd) - 1.0) / dur.count()),
			data_transfer,
			dur.count());
		}

	return 0;
}

int publisher_loop(int id, char* server, int port, int num_msgs, int msg_size, int num_subscribers) {
	Client* c = rtma_create_client(0, 0);
	rtma_client_connect(c, server, port);
	rtma_client_subscribe(c, MT_EXIT);
	rtma_client_subscribe(c, MT_SUBSCRIBER_READY);
	rtma_client_send_module_ready(c);

	rtma_client_send_signal(c, MT_PUBLISHER_READY);

	int subscribers_ready = 0;
	Message msg;
	while (subscribers_ready < num_subscribers) {
		if (rtma_client_read_message(c, &msg, BLOCKING)) {
			switch (MSG_TYPE(msg)) {
			case MT_SUBSCRIBER_READY:
				subscribers_ready++;
				break;
			}
		}
	}

	size_t packet_size = msg_size * sizeof(char);
	MDF_TEST_MSG* msg_data = (MDF_TEST_MSG*)malloc(packet_size);

	// Add some dummy data to send
	for (int i = 0; i < msg_size; i++) {
		msg_data->data[i] = i % 128;
	}

	auto start = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < num_msgs; i++) {
		int nbytes = rtma_client_send_message(c, MT_TEST_MSG, msg_data, packet_size);
	}

	rtma_client_send_signal(c, MT_PUBLISHER_DONE);

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> dur = end - start;

	double data_transfer = (double)num_msgs * (double)(msg_size + sizeof(RTMA_MSG_HEADER)) / (double)1024 / (double)1024 / dur.count();

	rtma_client_disconnect(c);
	rtma_destroy_client(&c);
	free(msg_data);

	printf("Publisher[%d] -> %d messages | %d messages/sec | %0.1lf MB/sec | %0.6lf sec\n",
		id,
		num_msgs,
		(int)((double)(num_msgs) / dur.count()),
		data_transfer,
		dur.count());

	return 0;
}

void usage(void) {
	printf("Usage: rtma-bench [-s server(127.0.0.1:7111)] [-np NUM_PUBLISHERS] [-ns NUM_SUBSCRIBERS] [-n NUM_MSGS] [-ms MESSAGE_SIZE]\n");

	printf("- h\n\tShow help message\n");
	printf("- ms int\n\tSize of the message. (default 128)\n");
	printf("- n int\n\tNumber of Messages to Publish(default 100000)\n");
	printf("- np int\n\tNumber of Concurrent Publishers(default 1)\n");
	printf("- ns int\n\tNumber of Concurrent Subscribers\n");
	printf("- s string\n\tRTMA message manager ip address (default 127.0.0.1)\n");
	printf("- p string\n\tRTMA message manager port (default 7111)\n");
}

int main(int argc, char** argv) {

	char server[] = "127.0.0.1";
	int num_publishers = 1;
	int num_subscribers = 1;
	int num_msgs = 100000;
	int msg_size = 128;
	int port = 7111;

	char* flag;

	const char* prog_name = argv[0];

	while (--argc > 0 && (*++argv)[0] == '-') {
		flag = &((*argv)[1]);

		if (strcmp(flag, "np") == 0) {
			num_publishers = atoi((*++argv));
			argc--;
		}
		else if (strcmp(flag, "ns") == 0) {
			num_subscribers = atoi((*++argv));
			argc--;
		}
		else if (strcmp(flag, "n") == 0) {
			num_msgs = atoi((*++argv));
			argc--;
		}
		else if (strcmp(flag, "ms") == 0) {
			msg_size = atoi((*++argv));
			argc--;
		}
		else if (strcmp(flag, "p") == 0) {
			port = atoi((*++argv));
			argc--;
		}
		else if (strcmp(flag, "h") == 0) {
			usage();
			return 0;
		}
		else {
			fprintf(stderr, "%s: unknown arg %s\n", prog_name, *argv);
			usage();
			return -1;
		}
	}

	// Main Thread RTMA module
	Client* c = rtma_create_client(0, 0);
	rtma_client_connect(c, server, port);
	rtma_client_subscribe(c, MT_EXIT);
	rtma_client_subscribe(c, MT_PUBLISHER_READY);
	rtma_client_subscribe(c, MT_SUBSCRIBER_DONE);
	rtma_client_send_module_ready(c);

	std::vector<std::thread> publishers;
	std::vector<std::thread> subscribers;

	printf("Initializing publisher threads...\n");

	for (int i = 0; i < num_publishers; i++)
		publishers.push_back(std::thread(publisher_loop, i + 1, server, port, num_msgs / num_publishers, msg_size, num_subscribers));

	// Wait for publisher threads to be established
	Message msg;
	int publishers_ready = 0;
	while (publishers_ready < num_publishers) {
		if (rtma_client_read_message(c, &msg, BLOCKING)) {
			switch (MSG_TYPE(msg)) {
			case MT_PUBLISHER_READY:
				publishers_ready++;
				break;
			}
		}
	}

	printf("Waiting for subscriber threads...\n");
	for (int i = 0; i < num_subscribers; i++)
		subscribers.push_back(std::thread(subscriber_loop, i + 1, server, port, num_msgs, msg_size));

	printf("Starting Test...\n");

	printf("Total RTMA Packet Size: %d bytes\n", sizeof(RTMA_MSG_HEADER) + msg_size);
	
	//Wait for subscribers to finish
	double abort_timeout = max(num_msgs / 10000.0, 10.0);
	auto abort_start = std::chrono::high_resolution_clock::now();

	int subscribers_done = 0;
	int publishers_done = 0;

	while ( (subscribers_done < num_subscribers) && (publishers_done < num_publishers) ) {
		if (rtma_client_read_message(c, &msg, 0.100)) {
			switch (MSG_TYPE(msg)) {
			case MT_SUBSCRIBER_DONE:
				subscribers_done++;
				break;
			case MT_PUBLISHER_DONE:
				publishers_done++;
				break;
			}
		}

		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> dur = now - abort_start;

		if (dur.count() > abort_timeout) {
			printf("Test Timeout! Sending Exit Signal...\n");
			rtma_client_send_signal(c, MT_EXIT);
			break;
		}
	}

	for (auto& publisher : publishers)
		publisher.join();

	for (auto& subscriber : subscribers)
		subscriber.join();

	printf("Done!\n");
	return 0;
}
