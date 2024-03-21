import struct 

RAW_FORMAT_VERSION = 1
NUM_ADC_CHANNELS = 8
ADC_MULT_FACTOR_16B_TEMP = 1000000
ADC_MULT_FACTOR_60V = 1000000
ADC_MULT_FACTOR_10V = 10000000

NTC_table = [
4140, 3521, 2902, 2593, 2394, 2249, 2136,
2045, 1969, 1903, 1846, 1795, 1750, 1709,
1672, 1638, 1607, 1578, 1550, 1525, 1501,
1479, 1457, 1437, 1418, 1400, 1383, 1366,
1350, 1335, 1321, 1307, 1293, 1280, 1268,
1256, 1244, 1233, 1222, 1211, 1201, 1191,
1181, 1171, 1162, 1153, 1144, 1136, 1127,
1119, 1111, 1104, 1096, 1088, 1081, 1074,
1067, 1060, 1054, 1047, 1041, 1034, 1028,
1022, 1016, 1010, 1004, 999, 993, 988, 982,
977, 972, 967, 961, 956, 952, 947, 942, 937,
933, 928, 924, 919, 915, 910, 906, 902, 898,
894, 889, 885, 881, 878, 874, 870, 866, 862,
859, 855, 851, 848, 844, 841, 837, 834, 830,
827, 824, 820, 817, 814, 811, 808, 804, 801,
798, 795, 792, 789, 786, 783, 780, 777, 775,
772, 769, 766, 763, 761, 758, 755, 752, 750,
747, 745, 742, 739, 737, 734, 732, 729, 727,
724, 722, 719, 717, 715, 712, 710, 707, 705,
703, 701, 698, 696, 694, 692, 689, 687, 685,
683, 681, 678, 676, 674, 672, 670, 668, 666,
664, 662, 660, 658, 656, 654, 652, 650, 648,
646, 644, 642, 640, 638, 636, 634, 632, 630,
628, 627, 625, 623, 621, 619, 617, 616, 614,
612, 610, 609, 607, 605, 603, 602, 600, 598,
596, 595, 593, 591, 590, 588, 586, 585, 583,
581, 580, 578, 576, 575, 573, 572, 570, 569,
567, 565, 564, 562, 561, 559, 558, 556, 555,
553, 552, 550, 548, 547, 546, 544, 543, 541,
540, 538, 537, 535, 534, 532, 531, 529, 528,
527, 525, 524, 522, 521, 520, 518, 517, 515,
514, 513, 511, 510, 509, 507, 506, 505, 503,
502, 501, 499, 498, 497, 495, 494, 493, 491,
490, 489, 487, 486, 485, 484, 482, 481, 480,
479, 477, 476, 475, 473, 472, 471, 470, 469,
467, 466, 465, 464, 462, 461, 460, 459, 458,
456, 455, 454, 453, 452, 450, 449, 448, 447,
446, 444, 443, 442, 441, 440, 439, 438, 436,
435, 434, 433, 432, 431, 430, 428, 427, 426,
425, 424, 423, 422, 421, 419, 418, 417, 416,
415, 414, 413, 412, 411, 409, 408, 407, 406,
405, 404, 403, 402, 401, 400, 399, 398, 397,
396, 394, 393, 392, 391, 390, 389, 388, 387,
386, 385, 384, 383, 382, 381, 380, 379, 378,
377, 376, 375, 374, 373, 372, 371, 370, 369,
368, 367, 366, 365, 364, 363, 362, 361, 360,
359, 358, 357, 356, 355, 354, 353, 352, 351,
350, 349, 348, 347, 346, 345, 344, 343, 342,
341, 340, 339, 338, 337, 336, 335, 334, 333,
332, 331, 330, 329, 328, 327, 326, 325, 325,
324, 323, 322, 321, 320, 319, 318, 317, 316,
315, 314, 313, 312, 311, 310, 310, 309, 308,
307, 306, 305, 304, 303, 302, 301, 300, 299,
298, 298, 297, 296, 295, 294, 293, 292, 291,
290, 289, 288, 287, 287, 286, 285, 284, 283,
282, 281, 280, 279, 278, 278, 277, 276, 275,
274, 273, 272, 271, 270, 269, 269, 268, 267,
266, 265, 264, 263, 262, 261, 261, 260, 259,
258, 257, 256, 255, 254, 254, 253, 252, 251,
250, 249, 248, 247, 246, 246, 245, 244, 243,
242, 241, 240, 239, 239, 238, 237, 236, 235,
234, 233, 233, 232, 231, 230, 229, 228, 227,
226, 226, 225, 224, 223, 222, 221, 220, 219,
219, 218, 217, 216, 215, 214, 213, 213, 212,
211, 210, 209, 208, 207, 207, 206, 205, 204,
203, 202, 201, 201, 200, 199, 198, 197, 196,
195, 194, 194, 193, 192, 191, 190, 189, 188,
188, 187, 186, 185, 184, 183, 182, 182, 181,
180, 179, 178, 177, 176, 176, 175, 174, 173,
172, 171, 170, 170, 169, 168, 167, 166, 165,
164, 164, 163, 162, 161, 160, 159, 158, 157,
157, 156, 155, 154, 153, 152, 151, 151, 150,
149, 148, 147, 146, 145, 145, 144, 143, 142,
141, 140, 139, 138, 138, 137, 136, 135, 134,
133, 132, 131, 131, 130, 129, 128, 127, 126,
125, 124, 124, 123, 122, 121, 120, 119, 118,
117, 117, 116, 115, 114, 113, 112, 111, 110,
109, 109, 108, 107, 106, 105, 104, 103, 102,
101, 101, 100, 99, 98, 97, 96, 95, 94, 93,
92, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83,
83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73,
72, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63,
62, 61, 60, 59, 58, 58, 57, 56, 55, 54, 53,
52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,
41, 40, 39, 38, 37, 36, 36, 35, 34, 33, 32,
31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21,
20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,
9, 8, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4,
-5, -6, -7, -8, -9, -10, -11, -12, -14, -15,
-16, -17, -18, -19, -20, -21, -22, -23, -24,
-26, -27, -28, -29, -30, -31, -32, -33, -34,
-36, -37, -38, -39, -40, -41, -42, -44, -45,
-46, -47, -48, -49, -51, -52, -53, -54, -55,
-57, -58, -59, -60, -61, -63, -64, -65, -66,
-68, -69, -70, -71, -73, -74, -75, -76, -78,
-79, -80, -81, -83, -84, -85, -87, -88, -89,
-91, -92, -93, -95, -96, -97, -99, -100,
-101, -103, -104, -106, -107, -108, -110,
-111, -113, -114, -116, -117, -118, -120,
-121, -123, -124, -126, -127, -129, -130,
-132, -133, -135, -136, -138, -140, -141,
-143, -144, -146, -148, -149, -151, -152,
-154, -156, -157, -159, -161, -163, -164,
-166, -168, -170, -171, -173, -175, -177,
-178, -180, -182, -184, -186, -188, -190,
-192, -194, -196, -198, -199, -201, -204,
-206, -208, -210, -212, -214, -216, -218,
-220, -223, -225, -227, -229, -232, -234,
-236, -239, -241, -243, -246, -248, -251,
-253, -256, -259, -261, -264, -267, -269,
-272, -275, -278, -281, -284, -287, -290,
-293, -296, -299, -302, -306, -309, -312,
-316, -319, -323, -327, -330, -334, -338,
-342, -346, -351, -355, -359, -364, -368,
-373, -378, -383, -388, -394, -399, -405,
-411, -417, -424, -430, -437, -445, -452,
-460, -469, -478, -487, -498, -509, -521,
-534, -548, -564, -583, -604, -629, -661,
-704, -774, -844
]

def decode_adc_data(adc_data_raw, data_len):
    adc_data = []
    for i in range(0, len(adc_data_raw), 2):
        value = struct.unpack('H', adc_data_raw[i:i+2])[0]
        adc_data.append(value)
    return adc_data

def decode_gpio_data(gpio_data_raw):
    return list(gpio_data_raw)

def read_file_header(file_path):
    """ Read and return the file header. """
    with open(file_path, 'rb') as file:
        # Read the settings (9 bytes) and adc_offsets (4 bytes * 8 channels)
        header_data = file.read(9 + 4 * 8)
        return header_data
    
def decode_time_data(time_data_raw, data_len):
    time_data = []
    for i in range(data_len):
        offset = i * 12
        entry = struct.unpack('8B I', time_data_raw[offset:offset + 12])
        time_data.append(entry)
    return time_data

def read_struct(file):
    msg_no = []
    # Read the struct header
    struct_header_format = '2B H 12x'  # Format string for unpacking.  Assuming msg_no (1 byte), data_len (2 bytes), padding (11 bytes)
    struct_header_data = file.read(struct.calcsize(struct_header_format))

    if not struct_header_data:
        return -1  # End of file reached
    elif len(struct_header_data) < struct.calcsize(struct_header_format):
        return "Incomplete or missing struct header."

    # Unpack the struct header
    startByte1, startByte2, data_len = struct.unpack(struct_header_format, struct_header_data)

    # check if this is spi_msg_1 or spi_msg_2
    if startByte1 == 0xFA and startByte2 == 0xFB:
        # Read time data
        time_data_raw = file.read(12 * data_len)
        if not time_data_raw:
            return -1  # End of file reached
        time_data = decode_time_data(time_data_raw, data_len)

        # Read GPIO data + 2 padding bytes
        gpio_data_raw = file.read(data_len + 2)
        if not gpio_data_raw:
            return -1  # End of file reached
        gpio_data = decode_gpio_data(gpio_data_raw[0:70])

        # Read ADC data
        adc_data_raw = file.read(2 * 8 * data_len)  # 2 bytes per channel, 8 channels, data_len entries
        if not adc_data_raw:
            return -1  # End of file reached
        adc_data = decode_adc_data(adc_data_raw, data_len)
    else:
        # this must be spi_msg_2 type
        # Edit here:
        gpio_data = gpio_data
    return data_len, time_data, gpio_data, adc_data




def decode_file_header(header_data):
    # Unpack the settings (7 bytes)
    settings_format = '9B'  # 7 bytes for settings
    settings = struct.unpack(settings_format, header_data[:9])

    # Unpack the adc_offsets (8 int32_t values)
    adc_offsets_format = '8i'  # 8 int32_t values
    adc_offsets = struct.unpack(adc_offsets_format, header_data[9:])

    return settings, adc_offsets

def convert_adc(settings, adc_offsets, adc_data, data_len):
    adc_channel_range, adc_channel_type = settings[1], settings[2]
    converted_values = []

    # loop through a total of data_len*8 adc channels
    for i in range(data_len*8):
        channel = i % 8  # Determine the channel number (0-7)
        adc_value = adc_data[i]
        # Define range and offset based on adc_channel_range
        if adc_channel_range & (1 << channel):
            offset = 126774848
            range_value = 2*offset
            
        else:
            offset = 151699029
            range_value = 2*offset
            
        is_ntc = (adc_channel_type >> channel) & 1
        


        if is_ntc:  # NTC input
            # if in 16 bits, reduce to 12 bits
            if settings[4] == 16:
                adc_value = adc_value >> 4
            p1 = NTC_table[adc_value >> 2]
            p2 = NTC_table[(adc_value >> 2) + 1]
            temperature = p1 - ((p1 - p2) * (adc_value & 0x0003)) // 4
            converted_values.append(temperature/10.0)
        else:  # Voltage input
            t1 = adc_value * (-1 * range_value)  # Note the minus for inverted input
            if settings[4] == 12:  # ADC_12_BITS
                t2 = (t1+(2048-adc_offsets[channel])) / 4095  
            else:  # ADC_16_BITS
                # 65520 is ADC max for 16 bits
                t2 = (t1+(32760-adc_offsets[channel])) / 65520  
            voltage = round(t2 + offset, 0)
            converted_values.append(voltage)

    return converted_values

def csv_write(dataAdc, dataGpio, date_time_list, data_len, adc_channel_type, 
              adc_channel_range, adc_channel_enable, gpio_channel_enable, 
              decimal_char, separator_char,  f):
    
    file_bytes_written = 0

    # Identify the last enabled ADC channel for correct comma handling
    last_enabled_adc = -1
    for i in reversed(range(NUM_ADC_CHANNELS)):
        if adc_channel_enable & (1 << i):
            last_enabled_adc = i
            break

    # Identify the last enabled GPIO for correct comma handling
    last_enabled_gpio = -1
    for i in reversed(range(6)):  # Assuming 6 GPIO channels
        if gpio_channel_enable & (1 << i):
            last_enabled_gpio = i
            break

    for j, date_time in enumerate(date_time_list):
        # Initialize the string buffer for this row
        filestrbuffer = ""
        # Print time stamp
        year, month, day, hours, minutes, seconds, _, _, subseconds = date_time
        filestrbuffer += f"20{year:02d}-{month:02d}-{day:02d} {hours:02d}:{minutes:02d}:{seconds:02d}.{subseconds:03d}{separator_char}"

        # Print ADC values
        for x in range(NUM_ADC_CHANNELS):
            if adc_channel_enable & (1 << x):  # Check if the ADC channel is enabled
                adc_value = dataAdc[j * NUM_ADC_CHANNELS + x]
                # Determine if a comma should follow based on whether this is the last item overall
                add_comma = x != last_enabled_adc or last_enabled_gpio != -1
                if adc_channel_type & (1 << x):  # Temperature sensor
                    filestrbuffer += f"{adc_value}{',' if add_comma else ''}"
                elif adc_channel_range & (1 << x):  # Range is 60V
                    filestrbuffer += f"{adc_value / ADC_MULT_FACTOR_60V:.6f}{',' if add_comma else ''}"
                else:  # Range is 10V
                    filestrbuffer += f"{adc_value / ADC_MULT_FACTOR_10V:.7f}{',' if add_comma else ''}"

        # Print GPIO values
        for x in range(6):  # Assuming 6 GPIO channels
            if gpio_channel_enable & (1 << x):  # Check if the GPIO channel is enabled
                gpio_val = (dataGpio[j] & (1 << x)) >> x
                add_comma = x != last_enabled_gpio  # Add a comma only if this isn't the last enabled GPIO
                filestrbuffer += f"{gpio_val}{separator_char if add_comma else ''}"

        filestrbuffer += "\n"  # End of line for this record

        # Print and write the constructed CSV line
        print(filestrbuffer)
        if f is not None:  # If a file object is provided, write to it
            len_written = f.write(filestrbuffer)
            if len_written < 0:
                return 0  # Return zero if writing to the file failed
            file_bytes_written += len_written

    return data_len  # Return the original data length





def format_time_data(time_data_entry):
    # Assuming time_data_entry is a tuple (year, month, day, hours, minutes, seconds, padding1, padding2, subseconds)
    year, month, day, hours, minutes, seconds, _, _, subseconds = time_data_entry
    subseconds = subseconds
    return f"{2000 + year}-{month:02d}-{day:02d} {hours:02d}:{minutes:02d}:{seconds:02d}.{subseconds:03d}"

def format_gpio_data(gpio_data_entry):
    # Format GPIO data into binary states
    return [int(bit) for bit in format(gpio_data_entry, '08b')[:6]]


def generate_header(adc_channel_type, adc_channel_enabled, gpio_channel_enabled, separator_char):
    headers = ["time(utc)"]  # Start with fixed header part
    # Iterate through ADC channels
    for i in range(8):  # Assuming 8 channels
        if (adc_channel_enabled >> i) & 1:  # Check if the channel is enabled
            channel_type = 'NTC' if (adc_channel_type >> i) & 1 else 'AIN'
            headers.append(f"{channel_type}{i+1}")
    
    # Iterate through GPIO channels
    for i in range(6):  # Assuming 6 GPIO channels
        if (gpio_channel_enabled >> i) & 1:  # Check if the channel is enabled
            headers.append(f"DI{i+1}")
    
    return separator_char.join(headers)

def read_spi_msg_1(file, rows_remaining):
    # Read the header for spi_msg_1
    try:
        startByte1, startByte2, data_len = struct.unpack('2B H', file.read(4))
    except struct.error:
        return -1  # End of file or error in reading
    
    if not (startByte1 == 0xFA and startByte2 == 0xFB):
        return "Unexpected start bytes for spi_msg_1."
    if not ( data_len<=70):
        return "Unexpected data length for spi_msg_1"
    
    # if data_len < 70, there are no padding bytes
    if (data_len == 70):
        if rows_remaining < data_len:
            data_len = rows_remaining

        file.read(12)  # Skip padding0

        # Read time data
        time_data_raw = file.read(12 * data_len)
        time_data = decode_time_data(time_data_raw, data_len)

        # Read GPIO data
        gpio_data_raw = file.read(data_len + 2)
        gpio_data = decode_gpio_data(gpio_data_raw[:-2])  # Exclude padding bytes

        # Read ADC data
        adc_data_raw = file.read(2 * 8 * data_len)
        adc_data = decode_adc_data(adc_data_raw, data_len)
    else:
        # Read time data
        time_data_raw = file.read(12 * data_len)
        time_data = decode_time_data(time_data_raw, data_len)

        # Read GPIO data
        gpio_data_raw = file.read(data_len)
        gpio_data = decode_gpio_data(gpio_data_raw)  # Exclude padding bytes

        # Read ADC data
        adc_data_raw = file.read(2 * 8 * data_len)
        adc_data = decode_adc_data(adc_data_raw, data_len)

    rows_remaining = rows_remaining - data_len

    return data_len, time_data, gpio_data, adc_data, rows_remaining

def read_spi_msg_2(file, data_len, rows_remaining):

    

    if (data_len == 70):
        if rows_remaining < data_len:
            data_len = rows_remaining
        # Skip to the adcData part directly, assuming we know its offset
        adc_data_raw = file.read(data_len*8*2)
        padding = file.read(2)
        gpio_data_raw = file.read(data_len)
        time_data_raw = file.read(data_len*12)
        padding = file.read(12)
    else:
        adc_data_raw = file.read(data_len*8*2)
        gpio_data_raw = file.read(data_len)
        time_data_raw = file.read(data_len*12)
   
    
    try:
        data_len, stopByte0, stopByte1 = struct.unpack('H 2B', file.read(4))
    except struct.error:
        return -1  # End of file or error in reading
        
    if not (stopByte0 == 0xFB and stopByte1 == 0xFA):
        return "Unexpected start bytes for spi_msg_2."
    if not ( data_len<=70):
        return "Unexpected data length for spi_msg_2"
    
    adc_data = decode_adc_data(adc_data_raw, data_len)

    # Read and process GPIO data
   
    gpio_data = decode_gpio_data(gpio_data_raw)  # Exclude padding bytes

    # Read and process time data
    time_data = decode_time_data(time_data_raw, data_len)

    rows_remaining = rows_remaining - data_len
    

    return data_len, time_data, gpio_data, adc_data, rows_remaining



# File path to your .dat file
file_path = 'C:/Users/ppott/Downloads/20240321_10-40-56_for-your-dat.dat'  # Replace with the actual file path
csv_file_path = file_path.rsplit('.dat', 1)[0] + '.csv' 

# Read and decode the file header
file_header = read_file_header(file_path)
decoded_settings, decoded_adc_offsets = decode_file_header(file_header)

# Display the decoded values
print("Settings:", decoded_settings)

# Assuming decoded_settings contains the settings as a tuple
# For example: decoded_settings = (0, 4, 255, 12, 7)

# Names of the settings
settings_names = ["File format version", "adc_channel_range", "adc_channel_type", "adc_channels_enabled", "adc_resolution", "log_sample_rate", "GPIO inputs enabled", "CSV decimal character", "CSV separator character"]

# Print the settings
for i, setting in enumerate(decoded_settings):
    print(f"{i+1}. {settings_names[i]}: {setting}")
    if (i==0 and (setting != RAW_FORMAT_VERSION)):
        print("ERROR: cannot convert %s. This data file is made with an older and incompatible Uberlogger firmware version.", file_path)
        exit()


print("ADC Offsets:", decoded_adc_offsets)
adc_channel_range = decoded_settings[1]
adc_channel_type = decoded_settings[2]  # Extract adc_channel_type from settings
adc_channel_enabled = decoded_settings[3]
gpio_channel_enabled = decoded_settings[6]
file_decimal_char = "." if decoded_settings[7] == 0 else ","
file_separator_char = "." if decoded_settings[8] == 0 else ";"

print(generate_header(adc_channel_type, adc_channel_enabled, gpio_channel_enabled, file_separator_char))  # Print the dynamically generated header


with open(file_path, 'rb') as file, open(csv_file_path, 'w+') as fcsv:
    header_size = 9 + 4 * 8  # Size of the header (9 bytes of settings + 8* adc_offsets)
    # Skip the header
    file.seek(header_size)

    # Seek to 4 bytes from the end of the file
    file.seek(-4, 2)  # The '2' argument means 'seek relative to file's end'
    # Read the last 4 bytes
    total_number_rows = file.read(4)
    rows_remaining = struct.unpack('<I', total_number_rows)[0]
    # go back
    file.seek(header_size)
    # write the csv header
    fcsv.write(generate_header(adc_channel_type, adc_channel_enabled, gpio_channel_enabled, file_separator_char) + '\r')

    message_type = 1  # Start with spi_msg_1
    while True:
        # result = read_struct(file)
        if message_type == 1:
            result = read_spi_msg_1(file, rows_remaining)
            if result == -1:
                break  # End of file reached
            elif isinstance(result, str):
                print(result)  # Error message
                break
            message_type = 2  # Switch to spi_msg_2 for the next iteration
        else:
            result = read_spi_msg_2(file, data_len, rows_remaining) # this is under the condition that read_spi_msg_1 is always called before read_spi_msg_2
            if result == -1:
                break  # End of file reached
            elif isinstance(result, str):
                print(result)  # Error message
                break
            message_type = 1  # Switch back to spi_msg_1 for the next iteration

        if result == -1:
            break  # End of file reached

        data_len, time_data, gpio_data, adc_data, rows_remaining = result

        
        # Convert ADC data
        converted_adc_values = convert_adc(decoded_settings, decoded_adc_offsets, adc_data, data_len)
        
        # Process and print each line of data
        # for i in range(data_len):
            # time_str = format_time_data(time_data[i])
            # gpio_states = format_gpio_data(gpio_data[i])
            # adc_values = converted_adc_values[i * 8:(i + 1) * 8]  # Extract ADC values for this timestamp
            # print(f"{time_str},{','.join(map(str, adc_values))},{','.join(map(str, gpio_states))}")
        
        csv_write(converted_adc_values, gpio_data, time_data, data_len, adc_channel_type, adc_channel_range, 
                  adc_channel_enabled, gpio_channel_enabled, file_decimal_char, file_separator_char, fcsv)

        print('Rows remaining:', {rows_remaining})

        if (rows_remaining == 0):
            print('No rows remaining. Done')
            break

