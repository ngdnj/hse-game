from PIL import Image, ImageDraw
def create_sprite(path, color, is_circle=False, radius=16):
    img = Image.new("RGBA", (radius*2, radius*2), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    if is_circle:
        draw.ellipse((0, 0, radius*2-1, radius*2-1), fill=color)
    else:
        draw.rectangle((0, 0, radius*2-1, radius*2-1), fill=color)
    img.save(path)
create_sprite("assets/enemy/chaser.png", (255, 50, 50, 255), is_circle=True, radius=16)
create_sprite("assets/enemy/shooter.png", (200, 100, 255, 255), is_circle=False, radius=16)
create_sprite("assets/player/idle.png", (100, 200, 250, 255), is_circle=False, radius=16)
print("done")
