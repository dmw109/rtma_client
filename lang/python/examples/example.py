import sys
import ctypes

sys.path.append("../")
import pyrtma

# Create a user defined message from a ctypes.Structure or basic ctypes
class USER_MESSAGE(ctypes.Structure):
    _fields_ = [
            ('str', ctypes.c_byte * 16),
            ('val', ctypes.c_double),
            ('arr', ctypes.c_int * 8)
            ]

# Choose a unique message type id number
MT_USER_MESSAGE = 1234

# Add the message definition to pyrtma.core module internal dictionary
breakpoint()
pyrtma.AddMessage(msg_name='USER_MESSAGE', msg_type=MT_USER_MESSAGE, msg_def=USER_MESSAGE)

def publisher(server='127.0.0.1:7111'):
    # Setup Client
    mod = pyrtma.rtmaClient()
    mod.connect(server_name=server)

    # Build a packet to send
    msg = USER_MESSAGE()
    py_string = b'Hello World'
    msg.str[:len(py_string)] = py_string
    msg.val = 123.456
    msg.arr[:] = list(range(8))

    while True:
        c = input('Hit enter to publish a message. (Q)uit.')
        if c not in ['Q', 'q']:
            mod.send_message(MT_USER_MESSAGE, msg)
            print('Sent a packet')
        else:
            mod.send_signal('EXIT')
            print('Goodbye')
            break


def subscriber(server='127.0.0.1:7111'):
    # Setup Client
    mod = pyrtma.rtmaClient()
    mod.connect(server_name=server)

    # Select the messages to receive
    mod.subscribe(['USER_MESSAGE', 'EXIT'])

    print('Waiting for packets...')
    msg = pyrtma.Message()
    while True:
        status= mod.read_message(msg, timeout=-1)        
        if status:
            if msg.msg_name == 'USER_MESSAGE':
                print(msg)
            elif msg.msg_name == 'EXIT':
                print('Goodbye.')
                break


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default='127.0.0.1:7111', help='RTMA Message Manager ip address.')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--pub', default=False, action='store_true', help='Run as publisher.')
    group.add_argument('--sub', default=False, action='store_true', help='Run as subscriber.')

    args = parser.parse_args()

    if args.pub:
        print('pyrtma Publisher')
        publisher(args.server)
    elif args.sub:
        print('pyrtma Subscriber')
        subscriber(args.server)
    else:
        print('Unknown input')
