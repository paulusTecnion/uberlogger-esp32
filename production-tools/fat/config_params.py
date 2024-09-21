# config_thresholds.py

# Define thresholds for AINx values based on resolution
AIN_THRESHOLDS = {
    16: 0.02,  # 16-bit resolution allows a margin of ±0.02
    12: 0.05,  # 12-bit resolution allows a margin of ±0.05
}

# DI values should always be 0 or 1, no margin needed

MIN_TIME_100HZ_TOLERANCE = -0.095
MAX_TIME_100HZ_TOLERANCE = 0.105

# Define server address
server_url = "http://192.168.4.1:80/ajax"
download_dir = "./downloaded_files"  # Directory to save downloaded files
convert_script_path = "../../front/www/convert_raw.py"  # Path to convert_raw.py