from PIL import Image, ImageDraw, ImageFont

# Create a blank image for testing
image = Image.new('RGB', (200, 100), color='white')
draw = ImageDraw.Draw(image)

# Specify font (ensure the path to the font is correct)
font = ImageFont.truetype("arial.ttf", 15)

# Get text size
text = "Hello, World!"
text_width, text_height = draw.textsize(text, font=font)

# Draw the text on the image
draw.text((10, 25), text, fill="black", font=font)

# Save the image
image.save('test_image.png')
