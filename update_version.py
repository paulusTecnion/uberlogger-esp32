import re
import datetime
import os

# Define major, minor, patch version numbers
MAJOR = 1
MINOR = 1
PATCH = 0

# Get the directory where the script is located
script_dir = os.path.dirname(os.path.realpath(__file__))
# Generate the path to sysinfo.h based on the script's location
sysinfo_path = os.path.join(script_dir, 'main', 'sysinfo.h')

def generate_version():
    # Get the current date and time
    now = datetime.datetime.now()
    
    # Format the date and time into a version string
    date_time_str = now.strftime("%Y.%m.%d.%H.%M")
    
    # Combine the major, minor, patch, and date-time to form the full version
    return f"{MAJOR}.{MINOR}.{PATCH}_{date_time_str}"

def update_version(new_version):
    with open(sysinfo_path, 'r') as f:
        content = f.readlines()
        
    # Regular expression pattern to find the line with SW_VERSION
    pattern = r'static const char SW_VERSION\[\] =  "(.*?)"'
    
    # New version line to replace
    new_version_line = f'static const char SW_VERSION[] =  "{new_version}";\n'
    
    # Flag to indicate if a line was replaced
    replaced = False
    
    for i, line in enumerate(content):
        if re.search(pattern, line):
            content[i] = new_version_line
            replaced = True
            break
            
    if not replaced:
        print("SW_VERSION line not found. Adding new line.")
        content.append(new_version_line)
    
    with open(sysinfo_path, 'w') as f:
        f.writelines(content)

# Generate a new version based on the major, minor, patch numbers and the current date and time
new_version = generate_version()

# Update the version in sysinfo.h
update_version(new_version)
