import os
import subprocess
import time
import fat_utils
from config_params import *
from fat_utils import *
import glob

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

    # don't make this sleep shorter. Apparently it's possible to request the file before the final write on the sd card is actually finished
    # after which you miss the last bytes. 
    time.sleep(3)

    # Retrieve the last .dat file
    last_file_name = retrieve_last_file()
    if last_file_name:
        file_path = download_file(last_file_name)
        if file_path:
            print(f"Test 4: PASS - File downloaded successfully: {file_path}")
            return file_path
    return False

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
                return csv_file
            else:
                print(f"Test 5: FAIL - CSV file not found after conversion")
                return False
        else:
            print(f"Test 5: FAIL - Conversion failed with error: {result.stderr}")
            return False
    except Exception as e:
        print(f"Test 5: FAIL - Exception occurred: {e}")
        return False

def cleanup_files(directory):
    # Define the patterns for the files to delete
    dat_files = glob.glob(os.path.join(directory, '*.dat'))
    csv_files = glob.glob(os.path.join(directory, '*.csv'))

    # Delete .dat files
    for file in dat_files:
        try:
            os.remove(file)
            print(f"Cleanup: Deleted {file}")
        except OSError as e:
            print(f"Cleanup: Failed to delete {file} - {e}")
    
    # Delete .csv files
    for file in csv_files:
        try:
            os.remove(file)
            print(f"Cleanup: Deleted {file}")
        except OSError as e:
            print(f"Cleanup: Failed to delete {file} - {e}")

    print("Cleanup: Completed.")