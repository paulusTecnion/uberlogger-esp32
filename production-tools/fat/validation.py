import os
import csv
import time
from datetime import datetime  # Import datetime module
from config_params import AIN_THRESHOLDS
from file_operations import *
from config import test_retrieve_and_set_settings

def test_validate_csv(csv_file, resolution_bits):
    expected_header = [
        "time(utc)", "AIN1", "AIN2", "AIN3", "AIN4", "AIN5", "AIN6", "AIN7", "AIN8",
        "DI1", "DI2", "DI3", "DI4", "DI5", "DI6"
    ]

    # Fetch the tolerance from the configuration
    tolerance = AIN_THRESHOLDS.get(resolution_bits)
    if tolerance is None:
        print(f"Test 6: FAIL - Unsupported resolution specified: {resolution_bits}")
        return False

    if not os.path.exists(csv_file):
        print("Test 6: FAIL - CSV file does not exist")
        return False

    try:
        with open(csv_file, newline='') as f:
            reader = csv.reader(f)
            header = next(reader)
            
            if header != expected_header:
                print("Test 6: FAIL - CSV header does not match expected format")
                print(f"Expected: {expected_header}")
                print(f"Found: {header}")
                return False
            
            print("Test 6: PASS - CSV header matches expected format")
            
            previous_time = None
            time_deltas = []
            timestamps = []
            
            for row in reader:
                if len(row) != len(expected_header):
                    print(f"Test 6: FAIL - Row length does not match expected format: {row}")
                    return False
                
                # Validate the timestamp format
                time_value = row[0]
                try:
                    current_time = datetime.strptime(time_value, "%Y-%m-%d %H:%M:%S.%f")
                except ValueError:
                    print(f"Test 6: FAIL - Invalid timestamp format: {time_value}")
                    return False
                
                # Store the timestamp for later reference
                timestamps.append(time_value)

                # Calculate time deltas to check the 100 Hz sampling rate
                if previous_time:
                    delta = (
                        (current_time - previous_time).total_seconds()
                    )
                    time_deltas.append((delta, previous_time, current_time))
                previous_time = current_time
                
                # Validate AIN values (within expected tolerance)
                for value in row[1:9]:  # AIN1 to AIN8
                    try:
                        ain_value = float(value)
                        if not -tolerance <= ain_value <= tolerance:
                            print(f"Test 6: FAIL - AIN value out of expected range ({-tolerance} to {tolerance}): {ain_value}")
                            return False
                    except ValueError:
                        print(f"Test 6: FAIL - AIN value is not a valid float: {value}")
                        return False
                
                # Validate DI values (should be 0)
                for value in row[9:]:  # DI1 to DI6
                    if value != '0':
                        print(f"Test 6: FAIL - DI value is not 0: {value}")
                        return False

            # Validate time deltas for approximately 100 Hz sampling
            for delta, prev_time, curr_time in time_deltas:
                if not MIN_TIME_100HZ_TOLERANCE <= delta <= MAX_TIME_100HZ_TOLERANCE:  # Allow some small variation around 0.01s (100 Hz)
                    print(f"Test 6: FAIL - Time step is not approximately 100 Hz: {delta} seconds")
                    print(f"Previous timestamp: {prev_time.strftime('%Y-%m-%d %H:%M:%S.%f')}")
                    print(f"Current timestamp: {curr_time.strftime('%Y-%m-%d %H:%M:%S.%f')}")
                    return False

            print(f"Test 6: PASS - CSV data rows are valid and within expected ranges for {resolution_bits}-bit resolution")
            return True

    except Exception as e:
        print(f"Test 6: FAIL - Exception occurred while reading CSV: {e}")
        return False

    
def test_for_12_bit_csv():
        # Assumes similar steps as test_convert_dat_file but for 12-bit configuration
    print("Setting up logger for 12-bit configuration...")

    # Modify logger configuration for 12-bit ADC resolution here
    test_retrieve_and_set_settings(11, 12)

    # This would involve setting the ADC_RESOLUTION to 12 and updating the settings

    last_file = test_logging_and_retrieve_file()
    if not last_file:
        return False
    csv_file = test_convert_dat_file(last_file)
    if not csv_file:
        return False
    if not test_validate_csv(csv_file, 12):
        return False
    return True
