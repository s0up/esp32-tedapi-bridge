#!/usr/bin/env python3

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), 'src/pypowerwall_example_reference'))

from tedapi import tedapi_pb2

def create_python_protobuf():
    """Create the exact same protobuf that Python pypowerwall creates"""
    
    # Create the main message
    pb = tedapi_pb2.Message()
    
    # Set the device name (DIN) - MATCH ACTUAL PYTHON CODE
    pb.message.deliveryChannel = 1
    pb.message.sender.local = 1
    pb.message.recipient.din = "1707000-11-L--TG1250700025WH"  # Python uses DIN, not local!
    
    # Create the payload with our minimal GraphQL query
    graphql_query = " query DeviceControllerQuery {\n  control {\n    systemStatus {\n        nominalFullPackEnergyWh\n        nominalEnergyRemainingWh\n    }\n  }\n}"
    
    pb.message.payload.send.num = 2
    pb.message.payload.send.payload.value = 1
    pb.message.payload.send.payload.text = graphql_query
    
    # Add the exact hardcoded auth code from Python
    auth_code = b'\x30\x81\x86\x02\x41\x14\xB1\x97\xA5\x7F\xAD\xB5\xBA\xD1\x72\x1A\xA8\xBD\x6A\xC5\x18\x98\x30\xB6\x12\x42\xA2\xB4\x70\x4F\xB2\x14\x76\x64\xB7\xCE\x1A\x0C\xFE\xD2\x56\x01\x0C\x7F\x2A\xF6\xE5\xDB\x67\x5F\x2F\x60\x0B\x16\x95\x5F\x71\x63\x13\x24\xD3\x8E\x79\xBE\x7E\xDD\x41\x31\x12\x78\x02\x41\x70\x07\x5F\xB4\x1F\x5D\xC4\x3E\xF2\xEE\x05\xA5\x56\xC1\x7F\x2A\x08\xC7\x0E\xA6\x5D\x1F\x82\xA2\xEB\x49\x7E\xDA\xCF\x11\xDE\x06\x1B\x71\xCF\xC9\xB4\xCD\xFC\x1E\xF5\x73\xBA\x95\x8D\x23\x6F\x21\xCD\x7A\xEB\xE5\x7A\x96\xF5\xE1\x0C\xB5\xAE\x72\xFB\xCB\x2F\x17\x1F'
    pb.message.payload.send.code = auth_code
    
    # Add the "b" field with value "{}"
    pb.message.payload.send.b.value = "{}"
    
    # Add tail field (Python reference sets pb.tail.value = 1)
    pb.tail.value = 1
    
    # Serialize the protobuf
    serialized = pb.SerializeToString()
    
    print(f"Python protobuf length: {len(serialized)} bytes")
    print("Python protobuf hex:")
    hex_str = ' '.join(f'{b:02X}' for b in serialized)
    print(hex_str)
    print()
    
    # Show structure breakdown
    print("Structure analysis:")
    print(f"- GraphQL query length: {len(graphql_query)}")
    print(f"- Auth code length: {len(auth_code)}")
    print(f"- DIN length: {len('1707000-11-L--TG1250700025WH')}")
    print(f"- Total protobuf: {len(serialized)} bytes")
    
    return serialized

if __name__ == "__main__":
    create_python_protobuf()