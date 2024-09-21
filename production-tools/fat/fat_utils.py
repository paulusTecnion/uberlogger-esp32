import requests
import os
from config_params import *

def test_connect_to_server():
    try:
        response = requests.get(f"{server_url}/getValues", timeout=10)
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
    

def send_post_request(url, payload):
    try:
        response = requests.post(url, json=payload)
        response.raise_for_status()
        return response
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")
        return None

# Add more utility functions as needed
def retrieve_last_file():
    try:
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
    except Exception as e:
        print(f"Test 4: FAIL - An error occurred during download: {e}")
        return False

def download_file(filename):
    try:
        response = requests.get(f"{server_url}/getFileList/{filename}", stream=True, timeout=5)  # Set timeout to 5 seconds
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
    except requests.exceptions.Timeout:
        print("Test 4: FAIL - Download request timed out.")
        return False
    except Exception as e:
        print(f"Test 4: FAIL - An error occurred during download: {e}")
        return False