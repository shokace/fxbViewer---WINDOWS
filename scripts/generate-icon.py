#!/usr/bin/env python3
"""Regenerate the original application icon. Pillow is only needed for this step."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1] / 'assets'
size = 1024
image = Image.new('RGBA', (size, size), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((32, 32, 992, 992), radius=220, fill='#101923')
for x, height in ((220, 180), (340, 400), (460, 270), (580, 500)):
    draw.rounded_rectangle((x, 490-height//2, x+66, 490+height//2), radius=33, fill='#6ad8b9')
draw.ellipse((530, 485, 825, 780), fill='#101923', outline='#e1eaf2', width=40)
draw.line((783, 738, 895, 850), fill='#e1eaf2', width=52)
image = image.resize((256, 256), Image.Resampling.LANCZOS)
image.save(root / 'fxbviewer.png')
image.save(root / 'fxbviewer.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
check = Image.new('RGBA',(48,48),(0,0,0,0)); d=ImageDraw.Draw(check)
d.line([(9,24),(19,34),(39,12)], fill='#10232b', width=6)
check.save(root / 'check.png')
