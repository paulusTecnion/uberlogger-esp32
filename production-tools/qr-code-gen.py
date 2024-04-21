import qrcode
from PIL import Image, ImageDraw, ImageFont
from brother_ql import BrotherQLRaster, create_label
from brother_ql.devicedependent import label_type_specs
from brother_ql.backends.helpers import send

# Step 1: Generate QR Code
serial_number = "123456789ABC"  # Example serial number
qr = qrcode.QRCode(
    version=1,
    error_correction=qrcode.constants.ERROR_CORRECT_L,
    box_size=10,
    border=4,
)
qr.add_data(serial_number)
qr.make(fit=True)

img_qr = qr.make_image(fill_color="black", back_color="white")
img_qr = img_qr.convert("RGBA")

# Step 2: Design the Label
# Assume label width of 12mm (actual print area might be smaller), height will be adjusted based on QR code size
# width_mm = 12
# height_mm = img_qr.height * width_mm / img_qr.width  # Adjust height based on QR code aspect ratio
# width, height = label_type_specs['29x90']['tape_size'], int(height_mm * 300 / 25.4)  # 300 DPI and mm to pixels conversion
# Define the label dimensions directly (as an example)
label_width_mm = 29  # Approximate width in millimeters
label_height_mm = 90  # Approximate height in millimeters

# Convert mm to pixels (assuming 300 DPI for the Brother QL series printers)
dpi = 300
width_pixels = int(label_width_mm * dpi / 25.4)  # Convert mm to inches with / 25.4, then multiply by DPI for pixels
height_pixels = int(label_height_mm * dpi / 25.4)
width_pixels = 306
height_pixels = 991
# Create a blank label
label_img = Image.new('RGBA', (width_pixels, height_pixels), "white")
draw = ImageDraw.Draw(label_img)

# Paste the QR code onto the label
qr_size = min(width_pixels // 3, height_pixels)
qr_img_resized = img_qr.resize((qr_size, qr_size))
label_img.paste(qr_img_resized, (0, (height_pixels - qr_size) // 2))

# Add the serial number text next to the QR code
font_size = 10  # Adjust as needed
font = ImageFont.truetype("arial.ttf", font_size)  # Adjust font and path as needed
# text_width, text_height = draw.text((0,0),serial_number, font=font)
draw.text(((qr_size + 10), (height_pixels) // 2), serial_number, fill="black", font=font)

# Step 3: Print the Label
label_img = label_img.convert("L")  # Convert to grayscale for printing

label_img.save('label_preview.png')


printer_name = "usb://0x04f9:0x2042"  # Adjust as needed. Use 'brother_ql -b linux_kernel list' to find
model = 'QL-700'  # Your printer model

qlr = BrotherQLRaster(model)
create_label(qlr, label_img, '29x90')  # Adjust '29x90' based on your label size

# # Use the backend appropriate for your OS. For example, for Linux, use 'linux_kernel'
# send(qlr.data, printer_name, backend='linux_kernel')
send(qlr.data, printer_name, backend_identifier='pyusb', blocking=True)
