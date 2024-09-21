import requests
import time

# Server base URL
BASE_URL = "http://192.168.4.1"

# Pages to navigate to
PAGES = ["config", "log", "liveview"]

# Delay between navigation steps (in seconds)
DELAY = 0.9

# Timeout for each request (in seconds)
TIMEOUT = 10

# Test duration in seconds (1 hour)
TEST_DURATION = 3600  # 60 minutes * 60 seconds

def connect_to_server():
    try:
        response = requests.get(f"{BASE_URL}/ajax/getValues", timeout=TIMEOUT)
        if response.status_code == 200:
            print("Connected to server successfully.")
            return True
        else:
            print(f"Failed to connect to server. Status code: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Failed to connect to server: {e}")
        return False

def start_logger():
    try:
        response = requests.post(f"{BASE_URL}/ajax/loggerStart", timeout=TIMEOUT)
        if response.status_code == 200:
            print("Logger started successfully.")
            return True
        else:
            print(f"Failed to start logger. Status code: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Failed to start logger: {e}")
        return False

def stop_logger():
    try:
        response = requests.post(f"{BASE_URL}/ajax/loggerStop", timeout=TIMEOUT)
        if response.status_code == 200:
            print("Logger stopped successfully.")
            return True
        else:
            print(f"Failed to stop logger. Status code: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Failed to stop logger: {e}")
        return False

def set_logger_config(sample_rate, log_mode):
    config = {
        "LOG_SAMPLE_RATE": sample_rate,
        "LOG_MODE": log_mode
    }
    try:
        response = requests.post(f"{BASE_URL}/ajax/setConfig", json=config, timeout=TIMEOUT)
        if response.status_code == 200:
            print(f"Logger configuration set to {sample_rate} Hz and mode {log_mode}.")
            return True
        else:
            print(f"Failed to set logger configuration. Status code: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Failed to set logger configuration: {e}")
        return False

def navigate_to_page(page):
    try:
        response = requests.get(f"{BASE_URL}/index.html?page={page}", timeout=TIMEOUT)
        if response.status_code == 200:
            print(f"Successfully navigated to {page} page.")
            return True
        else:
            print(f"Failed to navigate to {page} page. Status code: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Error navigating to {page} page: {e}")
        return False

def run_test(sample_rate, duration_seconds):
    # Set the logger configuration
    if not set_logger_config(sample_rate, log_mode=0):  # log_mode=0 sets it to RAW
        return False

    time.sleep(3)

    # Start the logger
    if not start_logger():
        return False

    start_time = time.time()

    try:
        while time.time() - start_time < duration_seconds:
            for page in PAGES:
                if not navigate_to_page(page):
                    print("Navigation error occurred. Attempting to stop the logger.")
                    stop_logger()
                    print("Stopping script due to navigation error.")
                    return False
                time.sleep(DELAY)
        return True
    except KeyboardInterrupt:
        print("Script interrupted. Attempting to stop the logger.")
        stop_logger()
        return False
    finally:
        stop_logger()
        time.sleep(5)

def main():
    if not connect_to_server():
        print("Exiting due to connection failure.")
        return 1

    # Run the test at 1 Hz for 1 hour
    print("Starting test at once per minute for 1 hour...")
    if not run_test(sample_rate=3, duration_seconds=TEST_DURATION):  # 0 corresponds to 1 Hz
        print("Test at once per minute failed.")
        return 1

    print("Test at once per minute completed successfully.")

    # Run the test at 250 Hz for 1 hour
    print("Starting test at 250 Hz for 1 hour...")
    if not run_test(sample_rate=12, duration_seconds=TEST_DURATION):  # 7 corresponds to 250 Hz
        print("Test at 250 Hz failed.")
        return 1

    print("Test at 250 Hz completed successfully.")

    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())
