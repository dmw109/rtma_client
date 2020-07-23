# rtma_client
RTMA client implementation in C. Basic functionality works for windows.  Still need to test for linux.  Work in progress...

The header file supports compilation to a windows dll or static lib when the appropriate macros are set. Set RTMA_C_EXPORTS and _DYNAMIC_LIB to compile to a .dll.
Defaults are configured to compile to a static lib on windows.

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

