import subprocess
import re
import requests
import time
import xml.etree.ElementTree as ET

def create_wifi_profile(ssid):
    """Create a Wi-Fi profile for an open network."""
    profile_xml = f"""<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
    <name>{ssid}</name>
    <SSIDConfig>
        <SSID>
            <name>{ssid}</name>
        </SSID>
    </SSIDConfig>
    <connectionType>ESS</connectionType>
    <connectionMode>auto</connectionMode>
    <MSM>
        <security>
            <authEncryption>
                <authentication>open</authentication>
                <encryption>none</encryption>
                <useOneX>false</useOneX>
            </authEncryption>
        </security>
    </MSM>
</WLANProfile>"""

    profile_name = f"{ssid}.xml"
    with open(profile_name, "w") as file:
        file.write(profile_xml)
    
    # Add the profile using netsh
    subprocess.run(f"netsh wlan add profile filename=\"{profile_name}\"", shell=True, check=True)


def scan_for_uberlogger_networks():
    """Scan for Wi-Fi networks and return Uberlogger networks if found."""
    try:
        output = subprocess.check_output("netsh wlan show networks", shell=True, text=True)
        # Look for SSIDs that start with 'Uberlogger-'
        uberlogger_networks = re.findall(r"SSID \d+ : (Uberlogger-[^\r\n]+)", output)
        return uberlogger_networks
    except subprocess.CalledProcessError as e:
        print(f"Failed to scan networks: {e}")
        return []

def connect_to_network(ssid):
    """Attempt to connect to a specified Wi-Fi network after creating a profile."""
    create_wifi_profile(ssid)  # Create the profile
    """Connect to a specified Wi-Fi network."""
    try:
        subprocess.run(f"netsh wlan connect name={ssid}", check=True, shell=True, text=True)
        print(f"Connected to {ssid}.")
    except subprocess.CalledProcessError as e:
        print(f"Failed to connect to {ssid}: {e}")

def post_set_time():
    """Send a POST request to set the time."""
    timestamp = int(time.time()*1000)
    try:
        response = requests.post("http://192.168.4.1/ajax/setTime", json={"TIMESTAMP": timestamp})
        if response.status_code == 200:
            print("Time set successfully.")
        else:
            print(f"Failed to set time: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

def post_calibrate():
    """Send a POST request to calibrate."""
    try:
        response = requests.post("http://192.168.4.1/ajax/calibrate")
        if response.status_code == 200:
            print("Calibration successful.")
        else:
            print(f"Failed to calibrate: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

def check_connection(ssid):
    """Check if still connected to the specified Wi-Fi network."""
    try:
        output = subprocess.check_output("netsh wlan show interfaces", shell=True, text=True)
        return ssid in output
    except subprocess.CalledProcessError as e:
        print(f"Failed to check connection status: {e}")
        return False

def main():
    while True:
        uberlogger_networks = scan_for_uberlogger_networks()
        if uberlogger_networks:
            ssid_to_connect = uberlogger_networks[0]  # Connect to the first Uberlogger network found
            connect_to_network(ssid_to_connect)
            time.sleep(3)
            post_set_time()
            # time.sleep(2)
            # post_calibrate()
            
            # Wait until disconnected
            print("Waiting for disconnection...")
            while check_connection(ssid_to_connect):
                time.sleep(5)
            print("Disconnected from the network.")
            
        else:
            print("No Uberlogger network found. Retrying in 5 seconds...")
            time.sleep(5)

if __name__ == "__main__":
    main()
