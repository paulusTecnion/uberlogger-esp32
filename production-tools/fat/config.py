from fat_utils import send_post_request
import requests
import time
from config_params import *
from file_operations import *



def test_set_time():
    current_time = int(time.time() * 1000)  # Unix timestamp in ms
    payload = {"TIMESTAMP": current_time}
    response = requests.post(f"{server_url}/setTime", json=payload)
    if response.status_code == 200:
        print("Test 2: PASS - Local time set successfully")
        time.sleep(2)  # Delay after setting time
        return True
    else:
        print(f"Test 2: FAIL - Setting local time failed, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False

def test_retrieve_and_set_settings():
    # Retrieve current settings
    response = requests.get(f"{server_url}/getConfig")
    if response.status_code == 200:
        time.sleep(2)  # Delay after getting the settings
        config = response.json()
        if config['ADC_RESOLUTION'] == 16:
            print("Test 3: PASS - ADC Resolution is set to 16 bits")
        else:
            print("Test 3: FAIL - ADC Resolution is not set to 16 bits")
            print(f"Response: {response.json()}")
            return False
        
        # Set new settings
        config['LOG_SAMPLE_RATE'] = 11  # 100 Hz
        config['LOG_MODE'] = 0  # RAW mode
        response = requests.post(f"{server_url}/setConfig", json=config)
        if response.status_code == 200:
            print("Test 3: PASS - Settings updated successfully")
            time.sleep(2)  # Delay after setting the settings
            # Verify settings
            response = requests.get(f"{server_url}/getConfig")
            if response.status_code == 200:
                new_config = response.json()
                if new_config['LOG_SAMPLE_RATE'] == 11 and new_config['LOG_MODE'] == 0:
                    print("Test 3: PASS - Settings verified successfully")
                    return True
                else:
                    print("Test 3: FAIL - Settings verification failed")
                    print(f"Response: {response.json()}")
                    return False
        else:
            print(f"Test 3: FAIL - Updating settings failed, status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
    else:
        print(f"Test 3: FAIL - Retrieving settings failed, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False


