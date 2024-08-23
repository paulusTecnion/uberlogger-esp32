from config import test_set_time, test_retrieve_and_set_settings
from file_operations import test_logging_and_retrieve_file, test_convert_dat_file
from validation import test_validate_csv, test_for_12_bit_csv
from fat_utils import test_connect_to_server
import sys
from file_operations import cleanup_files
from config_params import download_dir

def run_tests():
    all_tests_passed = True
    
    if not test_connect_to_server():
        all_tests_passed = False
    if not test_set_time():
        all_tests_passed = False
    if not test_retrieve_and_set_settings():
        all_tests_passed = False
    last_file = test_logging_and_retrieve_file()
    if not last_file:
        all_tests_passed = False
    csv_file = test_convert_dat_file(last_file)
    if not csv_file:
        all_tests_passed = False
    if not test_validate_csv(csv_file, 16):
        all_tests_passed = False
    if not test_for_12_bit_csv():
        all_tests_passed = False
    
    # Cleanup only if all tests passed
    if all_tests_passed:
        cleanup_files(download_dir)
    else:
        print("Cleanup skipped due to test failures.")
    
    if all_tests_passed:
        print("All tests are PASSED")

    return 0 if all_tests_passed else 1


if __name__ == "__main__":
    sys.exit(run_tests())
