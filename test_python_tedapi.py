#!/usr/bin/env python3
import sys
import os
import requests
import json
from urllib3.exceptions import InsecureRequestWarning
requests.packages.urllib3.disable_warnings(InsecureRequestWarning)

sys.path.append('src/pypowerwall_example_reference')
from tedapi import tedapi_pb2

# Test if Python TEDAPI works from this system
def test_python_tedapi():
    # Create the exact same message as the Python reference
    pb = tedapi_pb2.Message()
    pb.message.deliveryChannel = 1
    pb.message.sender.local = 1
    pb.message.recipient.din = '1707000-11-L--TG1250700025WH'  # Your DIN
    pb.message.payload.send.num = 2
    pb.message.payload.send.payload.value = 1

    # Use the compact GraphQL query
    pb.message.payload.send.payload.text = ' query DeviceControllerQuery {\n  control {\n    systemStatus {\n        nominalFullPackEnergyWh\n        nominalEnergyRemainingWh\n    }\n    islanding {\n        customerIslandMode\n        contactorClosed\n        microGridOK\n        gridOK\n    }\n    meterAggregates {\n      location\n      realPowerW\n    }\n    alerts {\n      active\n    }\n    batteryBlocks {\n      din\n      disableReasons\n    }\n  }\n  system {\n    time\n    sitemanagerStatus {\n      isRunning\n    }\n  }\n}'

    # Hardcoded auth code from Python reference
    auth_code = b'0\201\206\002A\024\261\227\245\177\255\265\272\321r\032\250\275j\305\030\2300\266\022B\242\264pO\262\024vd\267\316\032\f\376\322V\001\f\177*\366\345\333g_/`\v\026\225_qc\023$\323\216y\276~\335A1\022x\002Ap\a_\264\037]\304>\362\356\005\245V\301\177*\b\307\016\246]\037\202\242\353I~\332\317\021\336\006\033q\317\311\264\315\374\036\365s\272\225\215#o!\315z\353\345z\226\365\341\f\265\256r\373\313/\027\037'
    pb.message.payload.send.code = auth_code

    pb.message.payload.send.b.value = '{}'
    pb.tail.value = 1

    # Serialize
    data = pb.SerializeToString()
    print(f'Sending {len(data)} bytes to TEDAPI...')
    
    # Send to Powerwall (must be on the Powerwall network)
    url = 'https://192.168.91.1/tedapi/v1'
    
    try:
        # Create session like the Python reference
        session = requests.Session()
        session.verify = False  # Disable SSL verification for local Powerwall
        
        headers = {
            'Content-Type': 'application/octet-stream',
            'User-Agent': 'python-requests/2.32.3'
        }
        
        response = session.post(url, data=data, headers=headers, timeout=10)
        
        print(f'Response status: {response.status_code}')
        print(f'Response headers: {dict(response.headers)}')
        print(f'Response length: {len(response.content)}')
        
        if response.status_code == 200:
            print('Response hex:')
            content = response.content
            for i in range(0, len(content), 16):
                chunk = content[i:i+16]
                hex_str = ' '.join(f'{b:02X}' for b in chunk)
                ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
                print(f'{i:04X}: {hex_str:<48} {ascii_str}')
            
            # Try to parse the protobuf response
            try:
                resp_pb = tedapi_pb2.Message()
                resp_pb.ParseFromString(content)
                
                # Check for JSON in the response
                if resp_pb.message.payload.recv.text:
                    print('\nJSON Response:')
                    json_data = json.loads(resp_pb.message.payload.recv.text)
                    print(json.dumps(json_data, indent=2))
                else:
                    print('\nNo JSON text found in response')
                    
            except Exception as e:
                print(f'\nError parsing protobuf response: {e}')
        else:
            print(f'Error: HTTP {response.status_code}')
            print(f'Response: {response.text}')
            
    except Exception as e:
        print(f'Request failed: {e}')

if __name__ == '__main__':
    test_python_tedapi()
