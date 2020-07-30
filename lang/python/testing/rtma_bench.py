import sys
import ctypes
import time
import multiprocessing

sys.path.append('../')

def publisher_loop(pub_id=0, num_msgs=10000, msg_size=128, num_subscribers=1, server='127.0.0.1:7111'):
    import pyrtma
    MT_TEST = 5000
    TEST = create_test_msg(msg_size)
    # Register user defined message types
    if msg_size > 0:
        pyrtma.AddMessage(msg_name='TEST', msg_type=MT_TEST, msg_def=TEST)
    else:
        pyrtma.AddSignal(msg_name='TEST', msg_type=MT_TEST)

    pyrtma.AddSignal(msg_name='PUBLISHER_READY', msg_type=5001)
    pyrtma.AddSignal(msg_name='PUBLISHER_DONE', msg_type=5002)
    pyrtma.AddSignal(msg_name='SUBSCRIBER_READY', msg_type=5003)

    # Setup Client
    mod = pyrtma.rtmaClient()
    mod.connect(server_name=server)
    mod.send_module_ready()
    mod.subscribe('SUBSCRIBER_READY') 

    # Signal that publisher is ready
    mod.send_signal('PUBLISHER_READY')

    # Wait for the subscribers to be ready
    num_subscribers_ready = 0
    msg = pyrtma.Message()
    while num_subscribers_ready < num_subscribers:
        status = mod.read_message(msg,timeout=-1)
        if status:
            if msg.msg_name == 'SUBSCRIBER_READY':
                num_subscribers_ready += 1

    # Create TEST message with dummy data
    msg = TEST()
    if msg_size > 0:
        msg.data[:] = list(range(msg_size))

    # Send loop
    tic = time.perf_counter()
    for n in range(num_msgs):
        mod.send_message(MT_TEST, msg)
    toc = time.perf_counter()

    mod.send_signal('PUBLISHER_DONE')

    # Stats
    dur = toc - tic
    data_rate = (msg_size + pyrtma.HEADER_SIZE) * num_msgs / float(1048576) / dur
    print(f"Publisher[{pub_id}] -> {num_msgs} messages | {int(num_msgs/dur)} messages/sec | {data_rate:0.1f} MB/sec | {dur:0.6f} sec ")


def subscriber_loop(sub_id=0, num_msgs=100000, msg_size=128, server='127.0.0.1:7111'):
    import pyrtma
    MT_TEST = 5000
    TEST = create_test_msg(msg_size)
    # Register user defined message types
    if msg_size > 0:
        pyrtma.AddMessage(msg_name='TEST', msg_type=MT_TEST, msg_def=TEST)
    else:
        pyrtma.AddSignal(msg_name='TEST', msg_type=MT_TEST)

    pyrtma.AddSignal(msg_name='SUBSCRIBER_READY', msg_type=5003)
    pyrtma.AddSignal(msg_name='SUBSCRIBER_DONE', msg_type=5004)

    # Setup Client
    mod = pyrtma.rtmaClient()
    mod.connect(server_name=server)
    mod.send_module_ready()
    mod.subscribe(['TEST', 'EXIT'])
    mod.send_signal('SUBSCRIBER_READY')

    # Read Loop (Start clock after first TEST msg received)
    msg_count = 0
    msg = pyrtma.Message()
    while msg_count < num_msgs:
        status = mod.read_message(msg, timeout=-1)
        if status:
            if msg.msg_name == 'TEST':
                if msg_count == 0:
                    test_msg_size = msg.msg_size
                    tic = time.perf_counter()
                toc = time.perf_counter()
                msg_count += 1
            elif msg.msg_name == 'EXIT':
                break
            
    mod.send_signal('SUBSCRIBER_DONE')

    # Stats
    dur = toc - tic
    data_rate = (test_msg_size * num_msgs) / float(1048576) / dur
    if msg_count == num_msgs:
        print(f"Subscriber [{sub_id:d}] -> {msg_count} messages | {int((msg_count-1)/dur)} messages/sec | {data_rate:0.1f} MB/sec | {dur:0.6f} sec ")
    else:
        print(f"Subscriber [{sub_id:d}] -> {msg_count} ({int(msg_count/num_msgs *100):0d}%) messages | {int((msg_count-1)/dur)} messages/sec | {data_rate:0.1f} MB/sec | {dur:0.6f} sec ")



def create_test_msg(msg_size):
    class TEST(ctypes.Structure):
        _fields_ = [('data', ctypes.c_byte * msg_size)]
    return TEST


if __name__ == '__main__':
    import argparse
    import pyrtma

    # Configuration flags for bench utility
    parser = argparse.ArgumentParser(description='rtmaClient bench test utility')
    parser.add_argument('-ms', default=128, type=int, dest='msg_size', help='Messge size in bytes.')
    parser.add_argument('-n', default=100000, type=int, dest='num_msgs', help='Number of messages.')
    parser.add_argument('-np', default=1, type=int, dest='num_publishers', help='Number of concurrent publishers.')
    parser.add_argument('-ns', default=1, type=int, dest='num_subscribers', help='Number of concurrent subscribers.')
    parser.add_argument('-s',default='127.0.0.1:7111', dest='server', help='RTMA message manager ip address (default: 127.0.0.1:7111)')
    args = parser.parse_args()

    #Main Thread RTMA client
    mod = pyrtma.rtmaClient()
    mod.connect(server_name=args.server)
    mod.send_module_ready()

    pyrtma.AddSignal(msg_name='PUBLISHER_READY', msg_type=5001)
    pyrtma.AddSignal(msg_name='PUBLISHER_DONE', msg_type=5002)
    pyrtma.AddSignal(msg_name='SUBSCRIBER_READY', msg_type=5003)
    pyrtma.AddSignal(msg_name='SUBSCRIBER_DONE', msg_type=5004)

    mod.subscribe('PUBLISHER_READY')
    mod.subscribe('PUBLISHER_DONE')
    mod.subscribe('SUBSCRIBER_READY')
    mod.subscribe('SUBSCRIBER_READY')

    print("Initializing publisher processses...")
    publishers = []
    for n in range(args.num_publishers):
        publishers.append(
                multiprocessing.Process(
                    target=publisher_loop,
                    kwargs={
                        'pub_id': n+1,
                        'num_msgs': int(args.num_msgs/args.num_publishers),
                        'msg_size': args.msg_size, 
                        'num_subscribers': args.num_subscribers,
                        'server': args.server})
                    )
        publishers[n].start()

    # Wait for publisher processes to be established
    publishers_ready = 0
    msg = pyrtma.Message()
    while publishers_ready < args.num_publishers:
        status = mod.read_message(msg, timeout=-1)
        if status:
            if msg.msg_name == 'PUBLISHER_READY':
                publishers_ready += 1

    print('Waiting for subscriber processes...')
    subscribers = []
    for n in range(args.num_subscribers):
        subscribers.append(
                multiprocessing.Process(
                    target=subscriber_loop,
                    kwargs={
                        'sub_id': n+1,
                        'num_msgs': args.num_msgs,
                        'msg_size': args.msg_size, 
                        'server': args.server})
                    )
        subscribers[n].start()

    print("Starting Test...")
    print(f"RTMA packet size: {pyrtma.HEADER_SIZE + args.msg_size}")
    print(f'Sending {args.num_msgs} messages...')

    # Wait for subscribers to finish
    abort_timeout = max(args.num_msgs/1000, 10) #seconds
    abort_start = time.perf_counter()

    subscribers_done = 0
    publishers_done = 0
    while (subscribers_done < args.num_subscribers) and (publishers_done < args.num_publishers):
        status = mod.read_message(msg, timeout=0.100)
        if status:
            if msg.msg_name == 'SUBSCRIBER_DONE':
                subscriber_done += 1
            elif msg.msg_name == 'PUBLISHER_DONE':
                publishers_done += 1

        if (time.perf_counter() - abort_start) > abort_timeout: 
            time.sleep(1)
            mod.send_signal('EXIT')
            print('Test Timeout! Sending Exit Signal...')
            break

    for publisher in publishers:
        publisher.join()

    for subscriber in subscribers:
        subscriber.join()
    
    print('Done!')
