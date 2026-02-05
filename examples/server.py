from flask import Flask, send_file, abort
from PIL import Image
import io
import requests
import os

app = Flask(__name__)

TILE_URL_TEMPLATE = "http://mt0.google.com/vt/lyrs=y&hl=en&x={x}&y={y}&z={z}"

def convert_to_rgb565(im: Image.Image, byte_order: str) -> bytes:
    """Convert PIL RGB image to LVGL-compatible RGB565 bytes."""
    rgb565_bytes = bytearray()
    for r, g, b in im.getdata():
        val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        rgb565_bytes += val.to_bytes(2, byte_order)
    return bytes(rgb565_bytes)

@app.route("/<int:z>/<int:x>/<int:y>")
def tile(z, x, y):

    url = TILE_URL_TEMPLATE.format(z=z, x=x, y=y)
    try:
        resp = requests.get(url)
        resp.raise_for_status()
    except Exception as e:
        abort(404, description=f"Tile not found: {e}")

    # Open PNG with PIL and convert to RGB565
    im = Image.open(io.BytesIO(resp.content)).convert("RGB")
    rgb565_bytes = convert_to_rgb565(im, "big")


    return rgb565_bytes, 200, {"Content-Type": "application/octet-stream"}

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
