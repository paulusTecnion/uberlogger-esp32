import qrcode
from PIL import Image
from reportlab.lib.pagesizes import mm
from reportlab.pdfgen import canvas
from reportlab.graphics.barcode.eanbc import Ean13BarcodeWidget
from reportlab.graphics.shapes import Drawing
from reportlab.graphics import renderPDF
import requests
import json
import base64

# PDF creation function
def create_label_pdf(sn, ean_code, output_file):
       # Set PDF size (29x90mm)
    c = canvas.Canvas(output_file, pagesize=(90*mm, 29*mm))
    
    # Add text
    c.setFont("Helvetica-Bold", 16)
    c.drawString(5 * mm, 23 * mm, "Uberlogger")
    
    c.setFont("Helvetica-Bold", 28)
    c.drawString(5 * mm, 12 * mm, "UL01B")
    
    c.setFont("Helvetica", 12)
    c.drawString(5 * mm, 5 * mm, f"SN: {sn}")
    
    # Add QR code next to SN (without overlapping EAN)
    qr = qrcode.make(sn)
    qr.save("qr_temp.png")  # Save temporary QR code image
    c.drawImage("qr_temp.png", 35 * mm, 2 * mm, width=10*mm, height=10*mm)  # Position QR code next to SN
    
    # Create and add EAN-13 barcode (lower in the layout)
    barcode = Ean13BarcodeWidget(ean_code)
    barcode_drawing = Drawing(40 * mm, 10 * mm)
    barcode_drawing.add(barcode)
    
    # Position the EAN-13 barcode lower on the canvas
    renderPDF.draw(barcode_drawing, c, 50 * mm, 2 * mm)

    # Finalize the PDF
    c.showPage()
    c.save()
    
    print(f"PDF created at {output_file}")

# Function to send the PDF to PrintNode via API
def send_to_printnode(pdf_file, api_key):
    # Load the PDF file
    with open(pdf_file, 'rb') as file_data:
        file_content = file_data.read()

    # Prepare headers and data for PrintNode API
    headers = {
        "Authorization": f"Basic {base64.b64encode((api_key + ':').encode()).decode()}",
        "Content-Type": "application/json"
    }

    # Request available printers (you need the printer ID)
    printers_url = "https://api.printnode.com/printers"
    response = requests.get(printers_url, headers=headers)
    
    if response.status_code != 200:
        print(f"Error retrieving printers: {response.text}")
        return
    
    printers = response.json()
    if not printers:
        print("No printers found.")
        return
    
    # Use the first printer ID (adjust if needed)
    printer_id = printers[0]['id']
    print(f"Using printer ID: {printer_id}")

    # Prepare the print job data
    print_job_url = "https://api.printnode.com/printjobs"
    print_job_data = {
        "printerId": printer_id,
        "title": "Label Print",
        "contentType": "pdf_base64",
        "content": base64.b64encode(file_content).decode('utf-8'),
        "source": "python-script"
    }

    # Send the print job
    response = requests.post(print_job_url, headers=headers, data=json.dumps(print_job_data))

    if response.status_code == 200:
        print("Print job successfully sent to PrintNode.")
    else:
        print(f"Error sending print job: {response.text}")

# Main function to generate and print label
def main():
    # Replace with the serial number you want to print
    sn = "E2454A35"
    ean = "6150824096049"
    # Output PDF file path
    output_file = "label_output.pdf"
    
    # Create the label PDF
    create_label_pdf(sn, ean, output_file)
    
    # Replace with your actual PrintNode API key
    api_key = "H6vzyV7Qg8HDCwvj_ZQe96JRafYOQySeM64_e_KoHjc"  # Encode API key in base64
    
    # Send the generated PDF to PrintNode
    # send_to_printnode(output_file, api_key)

# Run the main function
if __name__ == "__main__":
    main()
