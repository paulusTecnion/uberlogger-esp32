import requests
import time
import os
import subprocess



# Ensure download directory exists
if not os.path.exists(download_dir):
    os.makedirs(download_dir)

def start_logging():
    response = requests.post(f"{server_url}/loggerStart")
    if response.status_code == 200:
        print("Test 4: PASS - Logging started successfully")
        return True
    else:
        print(f"Test 4: FAIL - Could not start logging, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False

def stop_logging():
    response = requests.post(f"{server_url}/loggerStop")
    if response.status_code == 200:
        print("Test 4: PASS - Logging stopped successfully")
        return True
    else:
        print(f"Test 4: FAIL - Could not stop logging, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False

def retrieve_last_file():
    response = requests.get(f"{server_url}/getFileList/")
    if response.status_code == 200:
        file_list = response.json().get('root', {})
        if file_list:
            last_file_info = file_list[str(len(file_list))]
            last_file_name = last_file_info['NAME']
            if last_file_name.endswith(".dat"):
                print(f"Test 4: PASS - Last file retrieved: {last_file_name}")
                return last_file_name
            else:
                print("Test 4: FAIL - Last file is not a .dat file")
        else:
            print("Test 4: FAIL - No files found on SD card")
    else:
        print(f"Test 4: FAIL - Could not retrieve file list, status code: {response.status_code}")
        print(f"Response: {response.text}")
    return False

def download_file(filename):
    response = requests.get(f"{server_url}/getFileList/{filename}", stream=True)
    if response.status_code == 200:
        file_path = os.path.join(download_dir, filename)
        with open(file_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        return file_path
    else:
        print(f"Test 4: FAIL - Failed to download the file, status code: {response.status_code}")
        print(f"Response: {response.text}")
        return False


# Test 1: Connect to server
def test_connect_to_server():
    try:
        response = requests.get(f"{server_url}/getValues")
        if response.status_code == 200:
            print("Test 1: PASS - Connected to server")
            return True
        else:
            print(f"Test 1: FAIL - Unable to connect to server, status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
    except Exception as e:
        print(f"Test 1: FAIL - Exception occurred: {e}")
        return False

# Test 2: Set local time and check response
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

# Test 3: Retrieve settings, change them, and verify
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

# Test 4: Start logging, stop after 15 seconds, and retrieve last file
def test_logging_and_retrieve_file():
     # Wait for 15 seconds
    time.sleep(2)
    # Start logging
    if not start_logging():
        return False
    
    # Wait for 15 seconds
    time.sleep(15)

    # Stop logging
    if not stop_logging():
        return False

    time.sleep(2)

    # Retrieve the last .dat file
    last_file_name = retrieve_last_file()
    if last_file_name:
        file_path = download_file(last_file_name)
        if file_path:
            print(f"Test 4: PASS - File downloaded successfully: {file_path}")
            return file_path
    return False

# Test 5: Convert .dat file and verify .csv generation
def test_convert_dat_file(dat_file):
    # Get the absolute path to the convert_raw.py script
    script_dir = os.path.abspath(os.path.dirname(convert_script_path))
    script_name = os.path.basename(convert_script_path)
    
    # Create the full path to the .dat file
    dat_file_abs_path = os.path.abspath(dat_file)

    try:
        result = subprocess.run(
            ["python", script_name, dat_file_abs_path],
            capture_output=True,
            text=True,
            cwd=script_dir  # Set the working directory to where the script is located
        )
        print(result.stdout)  # Print the stdout from the conversion script
        if result.returncode == 0:
            # CSV should be created in the same directory as the .dat file
            csv_file = dat_file_abs_path.replace('.dat', '.csv')
            if os.path.exists(csv_file):
                print(f"Test 5: PASS - Conversion successful, CSV file generated: {csv_file}")
                return True
            else:
                print(f"Test 5: FAIL - CSV file not found after conversion")
                return False
        else:
            print(f"Test 5: FAIL - Conversion failed with error: {result.stderr}")
            return False
    except Exception as e:
        print(f"Test 5: FAIL - Exception occurred: {e}")
        return False


# Main function to run tests
def run_tests():
    if not test_connect_to_server():
        return
    if not test_set_time():
        return
    if not test_retrieve_and_set_settings():
        return
    last_file = test_logging_and_retrieve_file()
    if last_file:
        test_convert_dat_file(last_file)

if __name__ == "__main__":
    run_tests()
