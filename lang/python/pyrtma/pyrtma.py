import ctypes
import os
import sys

# Platform Detection
is_windows = bool(sys.getwindowsversion()[0]) or (sys.platform in ("win32", "cygwin")) 
is_linux   = sys.platform.startswith("linux")
is_osx     = (sys.platform == "darwin")
is_posix   = (os.name == "posix")
is_android = False
is_32bit   = (sys.maxsize <= 2**32)

MODULE_ID = ctypes.c_short
HOST_ID = ctypes.c_short
MSG_TYPE = ctypes.c_int

MID_MESSAGE_MANAGER = 0
MID_COMMAND_MODULE = 1
MID_APPLICATION_MODULE = 2
MID_NETWORK_RELAY = 3
MID_STATUS_MODULE = 4
MID_QUICKLOGGER = 5
HID_LOCAL_HOST = 0
HID_ALL_HOSTS = 0x7FFF

NO_MESSAGE = 0
GOT_MESSAGE = 1
DEFAULT_ACK_TIMEOUT = 3.0
MAX_LOGGER_FILENAME_LENGTH = 256
BLOCKING = -1
NONBLOCKING = 0
MAX_DATA_BYTES = 4096

MAX_MODULES = 200
DYN_MOD_ID_START = 100
MAX_HOSTS = 5
MAX_MESSAGE_TYPES = 10000
MAX_RTMA_MSG_TYPE = 99
MAX_RTMA_MODULE_ID = 9
MAX_CONTIGUOUS_MESSAGE_DATA = 9000

if is_windows:
    sockfd_t = ctypes.c_ulonglong
else:
    sockfd_t = ctypes.c_int

class Client(ctypes.Structure):
    _fields_ = [
            ('sockfd', sockfd_t),
            ('serv_addr', ctypes.c_byte * 128),
            ('module_id', MODULE_ID),
            ('host_id', HOST_ID),
            ('start_time', ctypes.c_double),
            ('pid', ctypes.c_int),
            ('msg_count', ctypes.c_int),
            ('connected', ctypes.c_int),
            ('perf_counter_freq', ctypes.c_double)
            ]

class RTMA_MSG_HEADER(ctypes.Structure):
    _fields_ = [ 
        ('msg_type', ctypes.c_int),
        ('msg_count', ctypes.c_int),
        ('send_time', ctypes.c_double),
        ('recv_time', ctypes.c_double),
        ('src_host_id', HOST_ID),
        ('src_mod_id', MODULE_ID),
        ('dest_host_id', HOST_ID),
        ('dest_mod_id', MODULE_ID),
        ('num_data_bytes', ctypes.c_int),
        ('remaining_bytes', ctypes.c_int),
        ('is_dynamic', ctypes.c_int),
        ('reserved', ctypes.c_int)
    ]

HEADER_SIZE = ctypes.sizeof(RTMA_MSG_HEADER)

def bytes2str(raw_bytes):
    '''Helper to convert a ctypes bytes array of null terminated strings to a
    list'''
    return raw_bytes.decode('ascii').strip('\x00').split('\x00')


Signal = MSG_TYPE

class Message(ctypes.Structure):
    _fields_ = [
        ('rtma_header', RTMA_MSG_HEADER),
        ('data', ctypes.c_byte * MAX_DATA_BYTES)
    ]


    def __init__(self, msg_name=None, msg_type=None, signal=False, msg_def=None):
        self.msg_name = msg_name
        self.msg_size = 0
        self.data_ptr = None

    def print(self):
        _print_message(ctypes.byref(self))

    def __getattr__(self, name):
        if name == 'rtma_header':
            return self.rtma_header
        elif name == 'msg_name':
            return self.msg_name
        elif name == 'data':
            return self.data
        else: # Got through the cast pointer
            if self.data_ptr is not None:
                return getattr(self.data_ptr.contents, name)
            else:
                raise(ValueError, 'The data pointer is not valid for this message.')

    def __repr__(self):
        if self.rtma_header is None:
            return 'Empty Message'

        s = f'Type:\t{self.msg_name}\n'
        s+= '---Header---\n'
        for name, ctype in self.rtma_header._fields_:
            s+= f"{name}:\t {getattr(self.rtma_header, name)}\n"

        s += '\n'
        s += '---Data---\n'
        if self.data_ptr is not None:
            if hasattr(self.data_ptr.contents, '_fields_'):
                for name, ctype in self.data_ptr.contents._fields_:
                    try:
                        # Try to convert bytes to a string list
                        if getattr(self.data_ptr.contents, name)._type_ == ctypes.c_byte:
                            s += f"{name}:\t {bytes2str(bytes(getattr(self.data_ptr.contents, name)))}\n"
                        else:
                            if hasattr(getattr(self.data_ptr.contents, name), '_length_'): 
                                s += f"{name}:\t {getattr(self.data_ptr.contents, name)[:]}\n"
                            else:
                                s += f"{name}:\t {getattr(self.data_ptr.contents, name)}\n"
                    except AttributeError:
                        s += f"{name}:\t {getattr(self.data_ptr.contents, name)}\n"
            else:
                s += f"{name}:\t {self.data_ptr.contents}\n"

        return s

    def __str__(self):
        return self.__repr__()


# RTMA INTERNAL MESSAGE TYPES

ALL_MESSAGE_TYPES = 0x7FFFFFFF

MT = {}
MT['EXIT'] = 0
MT['KILL'] = 1
MT['ACKNOWLEDGE'] = 2
MT['FAIL_SUBSCRIBE'] = 6
MT['CONNECT'] = 13
MT['DISCONNECT'] = 14
MT['SUBSCRIBE'] = 15
MT['UNSUBSCRIBE'] = 16
MT['MM_ERROR'] = 83
MT['MM_INFO'] = 84
MT['PAUSE_SUBSCRIPTION'] = 85
MT['RESUME_SUBSCRIPTION'] = 86
MT['SHUTDOWN_RTMA'] = 17
MT['SHUTDOWN_APP'] = 17
MT['SAVE_MESSAGE_LOG'] = 56
MT['MESSAGE_LOG_SAVED'] = 57
MT['PAUSE_MESSAGE_LOGGING'] = 58
MT['RESUME_MESSAGE_LOGGING'] = 59
MT['RESET_MESSAGE_LOG'] = 60
MT['DUMP_MESSAGE_LOG'] = 61
MT['FORCE_DISCONNECT'] = 82
MT['MODULE_READY'] = 26
MT['SM_EXIT'] = 48
MT['LM_EXIT'] = 55
MT['MM_READY'] = 94
MT['LM_READY'] = 96

# Dictionary to lookup message name by message type id number
# This needs come after all the message defintions are defined
MT_BY_ID = {}
for key, value in MT.items():
    MT_BY_ID[value] = key

del key
del value

module = sys.modules[__name__]

# Module level functions to add new messages and signals

def AddMessage(msg_name, msg_type, msg_def=None, signal=False):
    '''Add a user message definition to the rtma module'''
    mt = getattr(module, 'MT')
    mt[msg_name] = msg_type
    mt_by_id = getattr(module, 'MT_BY_ID')
    mt_by_id[msg_type] = msg_name

    if not signal:
        setattr(module, msg_name, msg_def)
    else:
        setattr(module, msg_name, MSG_TYPE)


def AddSignal(msg_name, msg_type):
    AddMessage(msg_name, msg_type, msg_def=None, signal=True)

#START OF RTMA INTERNAL MESSAGE DEFINITIONS

SUBSCRIBE = MSG_TYPE
UNSUBSCRIBE = MSG_TYPE
PAUSE_SUBSCRIPTION = MSG_TYPE
RESUME_SUBSCRIPTION = MSG_TYPE
MM_ERROR = ctypes.POINTER(ctypes.c_char)
MM_INFO = ctypes.POINTER(ctypes.c_char)

class CONNECT(ctypes.Structure):
    _fields_ = [
        ('logger_status', ctypes.c_short), 
        ('daemon_status', ctypes.c_short)]


class FAIL_SUBSCRIBE(ctypes.Structure):
    _fields_ = [
        ('mod_id', MODULE_ID),
        ('reserved', ctypes.c_short),
        ('msg_type', MSG_TYPE)]


class FAILED_MESSAGE(ctypes.Structure):
    _fields_ = [
	('dest_mod_id', MODULE_ID),
	('reserved', ctypes.c_short * 3),
	('time_of_failure', ctypes.c_double),
	('msg_header', RTMA_MSG_HEADER)]


class FORCE_DISCONNECT(ctypes.Structure):
    _fields_ = [('mod_id', ctypes.c_int)]


class MODULE_READY(ctypes.Structure):
    _fields_ = [('pid', ctypes.c_int)]


class SAVE_MESSAGE_LOG(ctypes.Structure):
    _fields_ = [('pathname', ctypes.c_char * MAX_LOGGER_FILENAME_LENGTH),
            ('pathname_length', ctypes.c_int)]

# END OF RTMA INTERNAL MESSAGE DEFINTIONS

# Load the shared library into ctypes
# TODO: Add variants for other platforms
DLL_NAME = "rtma_c.dll"
DLL_PATH = os.path.join(
                    os.path.dirname("__file__"),
                    "..",
                    "..",
                    "..",
                    "lib",
                    "dll", 
                    DLL_NAME)

lib = ctypes.CDLL(os.path.abspath(DLL_PATH) )

# Use None for void return arguments
VOID = None

# CTYPES PROTOTYPES 

_create_client = ctypes.CFUNCTYPE(
        ctypes.POINTER(Client),
        MODULE_ID,
        HOST_ID)(
        ('rtma_create_client', lib), (
        (1, 'module_id', 0),
        (1, 'host_id', 0))
        )


_wait_for_acknowledgement = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        ctypes.POINTER(Message),
        ctypes.c_double)(
        ('rtma_client_wait_for_acknowledgement', lib), (
        (1, 'client'),
        (1, 'message'),
        (1, 'timeout', DEFAULT_ACK_TIMEOUT))
        )


_get_timestamp = ctypes.CFUNCTYPE(ctypes.c_double, ctypes.POINTER(Client))(
        ('rtma_client_get_timestamp', lib),
        ((1, 'client'), )
        )


_connect = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client),
        ctypes.c_char_p,
        ctypes.c_short)(
        ('rtma_client_connect', lib), (
        (1, 'client'),
        (1, 'server_name', b'127.0.0.1'),
        (1, 'port', 7111))
        )


_send_module_ready = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client))(
        ('rtma_client_send_module_ready', lib),
        ((1, 'client'), )
        )


_send_message = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        MSG_TYPE,
        ctypes.c_void_p,
        ctypes.c_size_t)(
        ('rtma_client_send_message', lib), (
        (1, 'client'),
        (1, 'msg_type'),
        (1, 'msg'),
        (1, 'len'))
        )


_send_message_to_module = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        MSG_TYPE,
        ctypes.c_void_p,
        ctypes.c_size_t,
        MODULE_ID,
        HOST_ID,
        ctypes.c_double)(
        ('rtma_client_send_message_to_module', lib), (
        (1, 'client'),
        (1, 'msg_type'),
        (1, 'msg'),
        (1, 'len'),
        (1, 'dest_mod_id'),
        (1, 'dest_host_id'),
        (1, 'timeout'))
        )


_send_signal = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        Signal)(
        ('rtma_client_send_signal', lib), (
        (1, 'client'),
        (1, 'signal'))
        )


_send_signal_to_module = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        Signal,
        MODULE_ID,
        HOST_ID,
        ctypes.c_double)(
        ('rtma_client_send_signal_to_module', lib), (
        (1, 'client'),
        (1, 'signal'),
        (1, 'dest_mod_id'),
        (1, 'dest_host_id'),
        (1, 'timeout'))
        )


_read_message = ctypes.CFUNCTYPE(
        ctypes.c_int,
        ctypes.POINTER(Client),
        ctypes.POINTER(Message),
        ctypes.c_double)(
        ('rtma_client_read_message', lib), (
        (1, 'client'),
        (1, 'message'),
        (1, 'timeout'))
        )


_disconnect = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client))(
        ('rtma_client_disconnect', lib),
        ((1, 'client'), )
        )


_subscribe = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client),
        MSG_TYPE)(
        ('rtma_client_subscribe', lib), (
        (1, 'client'),
        (1, 'msg_type'))
        )


_unsubscribe = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client),
        MSG_TYPE)(
        ('rtma_client_unsubscribe', lib), (
        (1, 'client'),
        (1, 'msg_type'))
        )


_pause_subscription = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client),
        MSG_TYPE)(
        ('rtma_client_pause_subscription', lib), (
        (1, 'client'),
        (1, 'msg_type'))
        )


_resume_subscription = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Client),
        MSG_TYPE)(
        ('rtma_client_resume_subscription', lib), (
        (1, 'client'),
        (1, 'msg_type'))
        )


_destroy_client = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(ctypes.POINTER(Client)))(
        ('rtma_destroy_client', lib),
        ((1, 'client'), )
        )


_print_message = ctypes.CFUNCTYPE(
        VOID,
        ctypes.POINTER(Message))(
        ('rtma_message_print', lib),
        ((1, 'message'), )
        )

# END OF CTYPES PROTOTYPES

# Wrapper class
class rtmaClient(object):

    def __init__(self, module_id=0, host_id=0):
       self._client_ptr = _create_client(module_id, host_id) 
       self.server = None

    @property
    def module_id(self):
        return self._client_ptr.contents.module_id

    @property
    def host_id(self):
        return self._client_ptr.contents.host_id

    @property
    def msg_count(self):
        return self._client_ptr.contents.msg_count

    @property
    def pid(self):
        return self._client_ptr.contents.pid

    @property
    def connected(self):
        return bool(self._client_ptr.contents.connected)

    def __del__(self):
        if self.connected:
            _disconnect(self._client_ptr)
            _destroy_client(self._client_ptr)

    def connect(self, 
                server_name='127.0.0.1:7111',
                logger_status=False,
                daemon_status=False):

        server, port = server_name.split(':')
        self.server = server
        self.port = int(port)
        _connect(self._client_ptr, self.server.encode('ascii'), ctypes.c_short(self.port))

    def disconnect(self):
        if self.connected:
            _disconnect(self._client_ptr)

    def send_module_ready(self):
        _send_module_ready(self._client_ptr)

    def _subscription_control(self, msg_list, ctrl_msg):
        if not isinstance(msg_list, list): 
            msg_list = [msg_list]

        for msg_name in msg_list:
            _subscribe(self._client_ptr, MT[msg_name])

    def subscribe(self, msg_list):
        if not isinstance(msg_list, list): 
            msg_list = [msg_list]

        for msg_name in msg_list:
            _subscribe(self._client_ptr, MT[msg_name])

    def unsubscribe(self, msg_list):
        if not isinstance(msg_list, list): 
            msg_list = [msg_list]

        for msg_name in msg_list:
            _unsubscribe(self._client_ptr, MT[msg_name])

    def pause_subscription(self):
        if not isinstance(msg_list, list): 
            msg_list = [msg_list]

        for msg_name in msg_list:
            _pause_subscription(self._client_ptr, MT[msg_name])

    def resume_subscription(self):
        if not isinstance(msg_list, list): 
            msg_list = [msg_list]

        for msg_name in msg_list:
            _resume_subscription(self._client_ptr, MT[msg_name])

    def send_signal(self, signal_name, dest_mod_id=0, dest_host_id=0, timeout=-1):
        signal = Signal(MT[signal_name])
        _send_signal_to_module(self._client_ptr, signal, dest_mod_id, dest_host_id, timeout)

    def send_message(self, msg_type, msg, dest_mod_id=0, dest_host_id=0, timeout=-1):
        # Verify that the module & host ids are valid
        if dest_mod_id < 0 or dest_mod_id > MAX_MODULES:
            raise Exception(f"rtmaClient::send_message: Got invalid dest_mod_id [{dest_mod_id}]")

        if dest_host_id < 0 or dest_host_id > MAX_HOSTS:
            raise Exception(f"rtmaClient::send_message: Got invalid dest_host_id [{dest_host_id}]")

        res = _send_message_to_module(self._client_ptr, 
                                        msg_type,
                                        ctypes.cast(ctypes.byref(msg),
                                        ctypes.c_void_p),
                                        ctypes.sizeof(msg),
                                        dest_mod_id, 
                                        dest_host_id,
                                        timeout)

        return res

    def read_message(self, msg, timeout=-1):
        res = _read_message(self._client_ptr, ctypes.byref(msg), timeout)
        if res:
            msg.msg_name = MT_BY_ID[msg.rtma_header.msg_type]
            msg.msg_size = HEADER_SIZE + msg.rtma_header.num_data_bytes
            
            if msg.rtma_header.num_data_bytes:
                # Set the data into the appropriate structure definition
                msg.data_ptr = ctypes.cast(msg.data, ctypes.POINTER(getattr(module, msg.msg_name)))

                # Copy the data (Alternative Idea)
                # data = getattr(module, msg.msg_name)()
                # ctypes.memmove(ctypes.addressof(data), ctypes.addressof(msg.data), msg.rtma_header.num_data_bytes)

            return GOT_MESSAGE
        else:
            msg.data_ptr = None
            return None
		
    def wait_for_acknowledgement(self, msg=Message(), timeout=DEFAULT_ACK_TIMEOUT):
        res = _wait_for_acknowledgement(self._client_ptr, ctypes.byref(msg), timeout)
