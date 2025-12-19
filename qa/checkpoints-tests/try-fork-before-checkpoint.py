#!/usr/bin/env python3
# Copyright (c) 2021-2025 DeckerSU, https://github.com/DeckerSU
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Copyright (c) 2017-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

# partially based on p2p-acceptblock.py test from ZCash QA rpc-tests

from io import BytesIO
import os
import shutil
from traceback import print_tb
from test_framework.comptool import wait_until
from test_framework.mininode import(
    CBlock,
    NetworkThread,
    NodeConn,
    NodeConnCB,
    msg_block,
    msg_ping,
    msg_pong,
    mininode_lock,
    uint256_from_str,
    CUnsignedSyncChkptMessage
)

import time
import logging
import sys

from test_framework.util import PortSeed, connect_nodes, get_rpc_proxy, hex_str_to_bytes, initialize_datadir, p2p_port, rpc_url, start_node

class TestNode(NodeConnCB):
    def __init__(self):
        NodeConnCB.__init__(self)
        self.create_callback_map()
        self.connection = None
        self.ping_counter = 1
        self.last_pong = msg_pong()
        self.conn_closed = False
        self.checkpoints = []

    def add_connection(self, conn):
        self.connection = conn

    # Spin until verack message is received from the node.
    # We use this to signal that our test can begin. This
    # is called from the testing thread, so it needs to acquire
    # the global lock.
    def wait_for_verack(self):
        while True:
            with mininode_lock:
                if self.verack_received:
                    return
            time.sleep(0.05)

    # Wrapper for the NodeConn's send_message function
    def send_message(self, message):
        self.connection.send_message(message)

    def on_close(self, conn):
        self.conn_closed = True

    def on_reject(self, conn, message):
        conn.rejectMessage = message

    def on_getdata(self, conn, message):
        self.last_getdata = message

    def on_tx(self, conn, message):
        self.last_tx = message

    def on_inv(self, conn, message):
        self.last_inv = message
    def on_notfound(self, conn, message):

        self.last_notfound = message

    def on_pong(self, conn, message):
        self.last_pong = message
    
        # Sync up with the node after delivery of a block
    def sync_with_ping(self, timeout=30):
        def received_pong():
            return (self.last_pong.nonce == self.ping_counter)
        self.connection.send_message(msg_ping(nonce=self.ping_counter))
        success = wait_until(received_pong, timeout)
        self.ping_counter += 1
        return success
    def on_checkpoint(self, conn, message):
        self.checkpoints.append(message)
        print(f"Checkpoint received: {message}")


def feed_node_with_blocks(filename, node_index, test_node, nodes):
    """Feed blocks from a hex file to a node via test_node connection."""
    blocks_file = open(filename, 'r')
    ht = 0
    for hex_block in blocks_file:
        ht += 1
        block = CBlock()
        f = BytesIO(hex_str_to_bytes(hex_block.strip()))
        block.deserialize(f)
        block.calc_sha256()
        print(f"Block #{ht} / {nodes[node_index].getblockcount()}: {block.hash}")
        test_node.send_message(msg_block(block))
        #time.sleep(0.01)
        test_node.sync_with_ping()
        #time.sleep(0.01)
    blocks_file.close()


def main():
    
    # logging.basicConfig(level=logging.DEBUG)
    # logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)
    print("Preparing test environment...")

    # ../../src/qt/komodo-qt -ac_name=PUNTEN -ac_reward=300000000 -ac_nk="96,5" -testnode=1

    NodeConn.MAGIC_BYTES = {

        "mainnet": b"\x29\xe4\x9d\xab",   # mainnet PUNTEN
    }

    # setup
    possible_paths = ['../../src/komodod', './src/komodod']
    binary = None
    for path in possible_paths:
        if os.path.exists(path):
            binary = path
            break
    if binary is None:
        print(f"Error: komodod binary not found in any of the following locations: {', '.join(possible_paths)}")
        sys.exit(1)
    
    os.environ["ZCASHD"] = binary
    # os.environ["PYTHON_DEBUG"] = "1"

    PortSeed.n = 777 # The seed to use for assigning port numbers (default: current process id)

    nodes_count = 2
    # this will create nodes_count directories in /tmp/node0, /tmp/node1, etc.
    for i in range(0, nodes_count):
        print(f"Preparing directory /tmp/node{i}")
        shutil.rmtree(f"/tmp/node{i}", True)
        initialize_datadir('/tmp', i)

    # start nodes with custom config file
    nodes = []
    for i in range(0, nodes_count):
        conf_name = f'/tmp/node{i}/komodo.conf'
        nodes.append(start_node(
            i,
            '/tmp',
            [
                "-ac_name=PUNTEN",
                "-ac_reward=300000000",
                "-ac_nk=96,5",
                "-testnode=1",
                f"-conf={conf_name}",
                "-debug=net",
                "-debug=chk",
                "-dnsseed=0",
                "-onlynet=ipv4"
                # "-connect=127.1.0.1:0" # don't allow connections to other nodes
            ]
        ))

    import_master_node_key_response = nodes[0].importprivkey("UqzFP1iqnowunSPZ3vVZMb9ybnpPM2XXGnNjd3cbaQsXdrc4SrCA", "master", False)
    expected_address = "RH7MEByvbjJMCPgGmdd1Vf8TJPgaEzvy5x"
    if import_master_node_key_response != expected_address:
        print(f"Error: Expected address {expected_address}, but got {import_master_node_key_response}")
        nodes[0].stop()
        sys.exit(1)

    test_node = TestNode()
    connections = []
    connections.append(NodeConn("127.0.0.1", p2p_port(0), nodes[0], test_node, "mainnet", 170014))
    test_node.add_connection(connections[0])
    NetworkThread().start()
    test_node.wait_for_verack()

    feed_node_with_blocks('blocks-133-node-0.hex', 0, test_node, nodes)

    getcheckpoint_result = nodes[0].getcheckpoint()
    print(getcheckpoint_result)
    # check that the fields of the result match the expected values
    expected_checkpoint = '01e3f8443011dfa653e507c37d69a9ddf03d99adef0c1d2b651df8a2f6102d53'
    expected_height = 123
    if getcheckpoint_result.get('checkpoint') != expected_checkpoint:
        print(f"Error: Expected checkpoint {expected_checkpoint}, but got {getcheckpoint_result.get('checkpoint')}")
        nodes[0].stop()
        sys.exit(1)
    if getcheckpoint_result.get('height') != expected_height:
        print(f"Error: Expected height {expected_height}, but got {getcheckpoint_result.get('height')}")
        nodes[0].stop()
        sys.exit(1)
    print(f"✓ Checkpoint verification passed: checkpoint={getcheckpoint_result.get('checkpoint')}, height={getcheckpoint_result.get('height')}")

    # Send checkpoint from checkpoints[0] if available
    # i.e. trying to send old checkpoint to the node
    # ValidateSyncCheckpoint Warning: checkpoint is old: new checkpoint height=1, existing checkpoint height=123 (possibly reorg)
    if len(test_node.checkpoints) > 0:
        test_node.send_message(test_node.checkpoints[0])
        test_node.sync_with_ping()
        print(f"Sent checkpoint: {test_node.checkpoints[0]}")

    # Send modified (invalid) checkpoint to the node
    # ERROR: CSyncCheckpoint::CheckSignature() : verify signature failed
    # WARNING: ProcessMessage: Failed to process received checkpoint: signature check.
    invalid_checkpoint = test_node.checkpoints[0]
    # Get the unsigned message, modify hashCheckpoint, and serialize back
    unsigned = invalid_checkpoint.checkpoint.get_unsigned()
    if unsigned:
        # Convert hex string to uint256
        invalid_hash_hex = "0000000000000000000000000000000000000000000000000000000000000000"
        invalid_hash_bytes = hex_str_to_bytes(invalid_hash_hex)
        unsigned.hashCheckpoint = uint256_from_str(invalid_hash_bytes)
        # Serialize the modified unsigned message back to vchMsg
        invalid_checkpoint.checkpoint.vchMsg = unsigned.serialize()
        # Reset cache so it will be recreated if needed
        invalid_checkpoint.checkpoint._unsigned = None
    
    test_node.send_message(invalid_checkpoint)
    test_node.sync_with_ping()
    print(f"Sent invalid checkpoint: {invalid_checkpoint}")

    connections[0].disconnect_node()
    connections.pop(0) # close p2p connection with first node

    connections.append(NodeConn("127.0.0.1", p2p_port(1), nodes[1], test_node, "mainnet", 170014))
    test_node.add_connection(connections[0])
    test_node.wait_for_verack()
    time.sleep(1)

    feed_node_with_blocks('blocks-137-node-1.hex', 1, test_node, nodes)
    connections[0].disconnect_node()
    connections.pop(0) # close p2p connection with second node

    print("Connecting nodes[0] and nodes[1]")
    connect_nodes(nodes[0], 1)
    time.sleep(1)
    node0_getinfo = nodes[0].getinfo()
    node1_getinfo = nodes[1].getinfo()
    print(f"Node #0: ht.{node0_getinfo['blocks']}")
    print(f"Node #1: ht.{node1_getinfo['blocks']}")
    # same height with sync-checkpoint (reorg of first nodeshould fail)
    assert(nodes[0].getblockcount() == 133)
    assert(nodes[1].getblockcount() == 137)
    assert(nodes[0].getbestblockhash() == "016bd30eb7aa834d2f46b9a3a7a4cf46c410f89b2ac756d720831b3fe716ed3f")
    assert(nodes[1].getbestblockhash() == "05415ff1b41f9ef5b517cc91b33e895baa3c9eb56e16b65db7ffc3d2b0faaca2")
    
    # now force the first node (master) to create one block and send checkpoint to second node
    print("Start mining on Node #0")
    nodes[0].setgenerate(True, 2)
    while True:
        print("Tick-tock, one second passed...")
        time.sleep(1)
        node0_getinfo = nodes[0].getinfo()
        if node0_getinfo['blocks'] > 133 + 1: # on the first block checkpoint on second node still will be 0
            nodes[0].setgenerate(False)        
            break
    
    time.sleep(1)
    node0_getinfo = nodes[0].getinfo()
    node1_getinfo = nodes[1].getinfo()
    print(f"Node #0: ht.{node0_getinfo['blocks']}")
    print(f"Node #1: ht.{node1_getinfo['blocks']}")
    assert(nodes[0].getbestblockhash() == nodes[1].getbestblockhash())
    print(f"Node #0 checkpoint: {nodes[0].getcheckpoint()}")
    print(f"Node #1 checkpoint: {nodes[1].getcheckpoint()}")
    assert(nodes[0].getcheckpoint() == nodes[1].getcheckpoint())
    print(f"✓ Checkpoint verification passed: checkpoint={nodes[0].getcheckpoint()}")


    [ c.disconnect_node() for c in connections ]

    # stopping spinned daemons 
    for i in range(0, nodes_count):
        nodes[i].stop()

if __name__ == '__main__':
    main()
