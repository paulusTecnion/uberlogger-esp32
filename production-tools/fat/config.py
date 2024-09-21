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


def test_retrieve_and_set_settings(sample_rate, resolution):
    # Retrieve current settings
    response = requests.get(f"{server_url}/getConfig")
    if response.status_code == 200:
        time.sleep(2)  # Delay after getting the settings
        config = response.json()

        # Check the current ADC resolution
        if config['ADC_RESOLUTION'] == resolution:
            print(f"Test: PASS - ADC Resolution is already set to {resolution} bits")
        else:
            print(f"Test: Setting ADC Resolution to {resolution} bits")
        
        # Set new settings
        config['LOG_SAMPLE_RATE'] = sample_rate  # Set to provided sample rate
        config['ADC_RESOLUTION'] = resolution  # Set to provided resolution

        response = requests.post(f"{server_url}/setConfig", json=config)
        if response.status_code == 200:
            print("Test: PASS - Settings updated successfully")
            time.sleep(2)  # Delay after setting the settings

            # Verify settings
            response = requests.get(f"{server_url}/getConfig")
            if response.status_code == 200:
                new_config = response.json()
                if new_config['LOG_SAMPLE_RATE'] == sample_rate and new_config['ADC_RESOLUTION'] == resolution:
                    print("Test: PASS - Settings verified successfully")
                    return True
                else:
                    print("Test: FAIL - Settings verification failed")
                    print(f"Expected: Sample Rate = {sample_rate}, Resolution = {resolution}")
                    print(f"Received: Sample Rate = {new_config['LOG_SAMPLE_RATE']}, Resolution = {new_config['ADC_RESOLUTION']}")
                    return False
        else:
            print(f"Test: FAIL - Updating settings failed, status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
    else:
        print(f"Test: FAIL - Retrieving settings failed, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False


