# rtma_client
RTMA client implementation in C. Basic functionality works for windows.  Still need to test for linux.  Work in progress...

## Usage
```C
#include "rtma_client.h"
#include <stdio.h>

int main(int argc, char** argv) {
	Client *c = rtma_client_create(0, 0);

	rtma_client_connect(c, "127.0.0.1", 7111);

	rtma_client_subscribe(c, MT_TEST_MSG);

	Message msg;
	if (rtma_client_read_message(c, &msg, BLOCKING)) {
		switch (MSG_TYPE(msg)) {
			case MT_TEST_MSG
				printf("Got Test Msg\n");
				break;
		}
	}
	rtma_client_disconnect(c);
	rtma_destroy_client(c);

	return (EXIT_SUCCESS);
}

```

