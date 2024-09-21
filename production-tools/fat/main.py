from config import test_set_time, test_retrieve_and_set_settings
from file_operations import test_logging_and_retrieve_file, test_convert_dat_file
from validation import test_validate_csv, test_for_12_bit_csv
from fat_utils import test_connect_to_server
import sys
from file_operations import cleanup_files
from config_params import download_dir

def run_tests():
    try:
        if not test_connect_to_server():
            print("Test failed: test_connect_to_server. Aborting further tests.")
            sys.exit(1)
        
        if not test_set_time():
            print("Test failed: test_set_time. Aborting further tests.")
            sys.exit(1)
        
        if not test_retrieve_and_set_settings(11, 16):  # 16 bit, 100 Hz
            print("Test failed: test_retrieve_and_set_settings. Aborting further tests.")
            sys.exit(1)
        
        last_file = test_logging_and_retrieve_file()
        if not last_file:
            print("Test failed: test_logging_and_retrieve_file. Aborting further tests.")
            sys.exit(1)
        
        csv_file = test_convert_dat_file(last_file)
        if not csv_file:
            print("Test failed: test_convert_dat_file. Aborting further tests.")
            sys.exit(1)
        
        if not test_validate_csv(csv_file, 16):
            print("Test failed: test_validate_csv. Aborting further tests.")
            sys.exit(1)
        
        if not test_for_12_bit_csv():
            print("Test failed: test_for_12_bit_csv. Aborting further tests.")
            sys.exit(1)
        
        # Cleanup only if all tests passed
        cleanup_files(download_dir)
        
        print("All tests PASSED")
        return 0
    except SystemExit as e:
            print(f"Caught exit with status {e.code}")

if __name__ == "__main__":
    sys.exit(run_tests())