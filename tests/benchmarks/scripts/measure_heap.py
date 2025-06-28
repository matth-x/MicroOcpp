import os
import sys
import requests
import paramiko
import base64
import traceback
import io
import json
import time
import pandas as pd
import subprocess
import threading


requests.packages.urllib3.disable_warnings() # avoid the URL to be printed to console

# Test case selection (commented out a few to simplify testing for now)
testcase_name_list = [
    'TC_B_06_CS',
    'TC_B_07_CS',
    'TC_B_09_CS',
    'TC_B_10_CS',
    'TC_B_11_CS',
    'TC_B_12_CS',
    'TC_B_13_CS',
    'TC_B_32_CS',
    'TC_B_34_CS',
    'TC_B_35_CS',
    'TC_B_36_CS',
    'TC_B_37_CS',
    'TC_B_39_CS',
    'TC_C_02_CS',
    'TC_C_04_CS',
    'TC_C_06_CS',
    'TC_C_07_CS',
    'TC_C_49_CS',
    'TC_E_01_CS',
    'TC_E_02_CS',
    'TC_E_03_CS',
    'TC_E_04_CS',
    'TC_E_05_CS',
    'TC_E_06_CS',
    'TC_E_07_CS',
    'TC_E_09_CS',
    'TC_E_13_CS',
    'TC_E_15_CS',
    'TC_E_17_CS',
    'TC_E_20_CS',
    'TC_E_21_CS',
    'TC_E_24_CS',
    'TC_E_25_CS',
    'TC_E_28_CS',
    'TC_E_29_CS',
    'TC_E_30_CS',
    'TC_E_31_CS',
    'TC_E_32_CS',
    'TC_E_33_CS',
    'TC_E_34_CS',
    'TC_E_35_CS',
    'TC_E_39_CS',
    'TC_F_01_CS',
    'TC_F_02_CS',
    'TC_F_03_CS',
    'TC_F_04_CS',
    'TC_F_05_CS',
    'TC_F_06_CS',
    'TC_F_07_CS',
    'TC_F_08_CS',
    'TC_F_09_CS',
    'TC_F_10_CS',
    'TC_F_11_CS',
    'TC_F_12_CS',
    'TC_F_13_CS',
    'TC_F_14_CS',
    'TC_F_20_CS',
    'TC_F_23_CS',
    'TC_F_24_CS',
    'TC_F_26_CS',
    'TC_F_27_CS',
    'TC_G_01_CS',
    'TC_G_02_CS',
    'TC_G_03_CS',
    'TC_G_04_CS',
    'TC_G_05_CS',
    'TC_G_06_CS',
    'TC_G_07_CS',
    'TC_G_08_CS',
    'TC_G_09_CS',
    'TC_G_10_CS',
    'TC_G_11_CS',
    'TC_G_12_CS',
    'TC_G_13_CS',
    'TC_G_14_CS',
    'TC_G_15_CS',
    'TC_G_16_CS',
    'TC_G_17_CS',
    'TC_J_07_CS',
    'TC_J_08_CS',
    'TC_J_09_CS',
    'TC_J_10_CS',
]

# Result data set
df = pd.DataFrame(columns=['FN_BLOCK', 'Testcase', 'Pass', 'Heap usage (Bytes)'])
df.index.names = ['TC_ID']

max_memory_total = 0
min_memory_base = 1000 * 1000 * 1000

def cleanup_simulator():

    print('Clean up Simulator')

    print('   - stop Simulator, if still running')
    os.system('killall -s SIGINT mo_simulator')

    print('   - clean state')
    os.system('rm -rf ' + os.path.join('MicroOcppSimulator', 'mo_store', '*'))

    print('   - done')

def setup_simulator():

    cleanup_simulator()

    print('Setup Simulator')

    print('   - set credentials')

    with open(os.path.join('MicroOcppSimulator', 'mo_store', 'simulator.jsn'), 'w') as f:
        f.write(os.environ['MO_SIM_CONFIG'])
    with open(os.path.join('MicroOcppSimulator', 'mo_store', 'ws-conn-v201.jsn'), 'w') as f:
        f.write(os.environ['MO_SIM_OCPP_SERVER'])
    with open(os.path.join('MicroOcppSimulator', 'mo_store', 'rmt_ctrl.jsn'), 'w') as f:
        f.write(os.environ['MO_SIM_RMT_CTRL_CONFIG'])
    with open(os.path.join('MicroOcppSimulator', 'mo_store', 'rmt_ctrl.pem'), 'w') as f:
        f.write(os.environ['MO_SIM_RMT_CTRL_CERT'])

    print('   - start Simulator')

    os.system(os.path.join('MicroOcppSimulator', 'build', 'mo_simulator') + ' &')

    print('   - done')

# Ensure Simulator is still running despite Reset requests via OCPP or the rmt_ctrl interface
keepalive_simulator = False

def keepalive_simulator_thread():

    while keepalive_simulator:
        # Check if mo_simulator process can be found
        try:
            subprocess.check_output(['pgrep', '-f', 'mo_simulator'])
            # Found, still running
        except subprocess.CalledProcessError:
            # Exited, restart
            print('Keepalive: restart Simulator')
            os.system(os.path.join('MicroOcppSimulator', 'build', 'mo_simulator') + ' &')

        time.sleep(1)

def run_measurements():
    
    global max_memory_total
    global min_memory_base
    global keepalive_simulator
    
    print("Fetch TCs from Test Driver")

    response = requests.get(os.environ['TEST_DRIVER_URL'] + '/ocpp2.0.1/CS/testcases/' + os.environ['TEST_DRIVER_CONFIG'], 
                            headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})

    #print(json.dumps(response.json(), indent=4))

    testcases = []

    for i in response.json()['data']['testcasesData']:
        for j in i['data']:
            is_core = False
            for k in j['certification_profiles']:
                if k == 'Core':
                    is_core = True
                    break

            select = False
            for k in testcase_name_list:
                if j['testcase_name'] == k:
                    select = True
                    break

            if select:
                print(i['header'] + ' --- ' + j['functional_block'] + ' --- ' + j['description'])
                testcases.append(j)

    print('Get Simulator base memory data')
    setup_simulator()
    time.sleep(1)

    response = requests.post('http://localhost:8000/api/memory/reset')
    print(f'Simulator API /memory/reset:\n > {response.status_code}')

    response = requests.get('http://localhost:8000/api/memory/info')
    print(f'Simulator API /memory/info:\n > {response.status_code}, current heap={response.json()["total_current"]}, max heap={response.json()["total_max"]}')
    base_memory_level = response.json()["total_max"]
    min_memory_base = min(min_memory_base, response.json()["total_max"])

    print("Start Test Driver")

    response = requests.post(os.environ['TEST_DRIVER_URL'] + '/ocpp2.0.1/CS/session/start/' + os.environ['TEST_DRIVER_CONFIG'], 
                             headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})
    print(f'Test Driver /*/*/session/start/*:\n > {response.status_code}')
    #print(json.dumps(response.json(), indent=4))

    for testcase in testcases:
        print('\nRun ' + testcase['functional_block'] + ' > ' + testcase['description'] + ' (' + testcase['testcase_name'] + ')')

        if testcase['testcase_name'] in df.index:
            print('Test case already executed - skip')
            continue

        setup_simulator()
        time.sleep(1)

        # Check connection
        simulator_connected = False
        for i in range(5):
            response = requests.get(os.environ['TEST_DRIVER_URL'] + '/sut_connection_status', 
                                    headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})
            print(f'Test Driver /sut_connection_status:\n > {response.status_code}')
            #print(json.dumps(response.json(), indent=4))
            if response.status_code == 200 and response.json()['isConnected']:
                simulator_connected = True
                break
            else:
                print(f'Waiting for the Simulator to connect ({i}) ...')
                time.sleep(3)
        
        if not simulator_connected:
            print('Simulator could not connect to Test Driver')
            raise Exception()
        
        response = requests.post('http://localhost:8000/api/memory/reset')
        print(f'Simulator API /memory/reset:\n > {response.status_code}')

        # Keepalive Simulator while running the test cases (tests may triger Reset commands)
        keepalive_simulator = True
        keepalive_thread = threading.Thread(target=keepalive_simulator_thread, daemon=True)
        keepalive_thread.start()
        
        test_response = requests.post(os.environ['TEST_DRIVER_URL'] + '/testcases/' + testcase['testcase_name'] + '/execute', 
                                 headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})
        print(f'Test Driver /testcases/{testcase["testcase_name"]}/execute:\n > {test_response.status_code}')
        #try:
        #    print(json.dumps(test_response.json(), indent=4))
        #except:
        #    print(' > No JSON')

        keepalive_simulator = False
        keepalive_thread.join()

        sim_response = requests.get('http://localhost:8000/api/memory/info')
        print(f'Simulator API /memory/info:\n > {sim_response.status_code}, current heap={sim_response.json()["total_current"]}, max heap={sim_response.json()["total_max"]}')

        df.loc[testcase['testcase_name']] = [testcase['functional_block'], testcase['description'], 'x' if test_response.status_code == 200 and test_response.json()['data'][0]['verdict'] == "pass" else '-', str(sim_response.json()["total_max"] - min(base_memory_level, sim_response.json()["total_current"]))]

        max_memory_total = max(max_memory_total, sim_response.json()["total_max"])
        min_memory_base = min(min_memory_base, sim_response.json()["total_current"])

        #if False and test_response.json()['data'][0]['verdict'] != "pass":
        #    print('Test failure, abort')
        #    break

    print("Stop Test Driver")
    
    response = requests.post(os.environ['TEST_DRIVER_URL'] + '/session/stop', 
                             headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})
    print(f'Test Driver /session/stop:\n > {response.status_code}')
    #print(json.dumps(response.json(), indent=4))

    cleanup_simulator()

    print('Store test results')

    # Add some meta information
    max_memory = 0
    for index, row in df.iterrows():
        memory = row['Heap usage (Bytes)']
        if memory.isdigit():
            max_memory = max(max_memory, int(memory))

    functional_blocks = set()
    for index, row in df.iterrows():
        functional_blocks.add(row['FN_BLOCK'])

    print(functional_blocks)

    for i in functional_blocks:
        df.loc[f'TC_{i[0]}'] = [i, f'**{i}**', ' ', ' ']

    df.loc['|MO_SIM_000'] = ['-', '**Simulator stats**', ' ', ' ']
    df.loc['|MO_SIM_010'] = ['-', 'Base memory occupation', ' ', str(min_memory_base)]
    df.loc['|MO_SIM_020'] = ['-', 'Test case maximum', ' ', str(max_memory)]
    df.loc['|MO_SIM_030'] = ['-', 'Total memory maximum', ' ', str(max_memory_total)]

    df.sort_index(inplace=True)
    
    print(df)

    df.to_csv('docs/assets/tables/heap_v201.csv',index=False,columns=['Testcase','Pass','Heap usage (Bytes)'])

    print('Stored test results to CSV')

def run_measurements_and_retry():

    if (    'TEST_DRIVER_URL'        not in os.environ or
            'TEST_DRIVER_CONFIG'     not in os.environ or
            'TEST_DRIVER_KEY'        not in os.environ or
            'MO_SIM_CONFIG'          not in os.environ or
            'MO_SIM_OCPP_SERVER'     not in os.environ or
            'MO_SIM_RMT_CTRL_CONFIG' not in os.environ or
            'MO_SIM_RMT_CTRL_CERT'   not in os.environ):
        sys.exit('\nCould not read environment variables')

    n_tries = 3

    for i in range(n_tries):

        try:
            run_measurements()
            print('\n **Test cases executed successfully**')
            break
        except:
            print(f'Error detected ({i+1})')

            traceback.print_exc()

            print("Stop Test Driver")    
            response = requests.post(os.environ['TEST_DRIVER_URL'] + '/session/stop', 
                                    headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']})
            print(f'Test Driver /session/stop:\n > {response.status_code}')
            #print(json.dumps(response.json(), indent=4))

            cleanup_simulator()

            if i + 1 < n_tries:
                print('Retry test cases')
            else:
                print('\n **Test case execution aborted**')
                sys.exit('\nError running test cases')

run_measurements_and_retry()
