import os
import shutil
from PIL import Image

def main():
    master_png = r"C:\Users\Mechatronics\.gemini\antigravity-ide\brain\0123a2ed-f570-46dd-bcff-cc7767f41f3b\horse_icon_master.png"
    root_dir = r"c:\Users\Mechatronics\Desktop\Remotepc"
    
    print("Loading master icon...")
    im = Image.open(master_png)
    
    # 1. Save root icon.png
    root_png = os.path.join(root_dir, "icon.png")
    im.save(root_png, "PNG")
    print(f"Saved: {root_png}")
    
    # 2. Save root icon.ico
    ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    root_ico = os.path.join(root_dir, "icon.ico")
    im.save(root_ico, format="ICO", sizes=ico_sizes)
    print(f"Saved: {root_ico}")
    
    # 3. Save client/icon.png and client/icon.ico
    client_dir = os.path.join(root_dir, "client")
    client_png = os.path.join(client_dir, "icon.png")
    client_ico = os.path.join(client_dir, "icon.ico")
    shutil.copy2(root_png, client_png)
    shutil.copy2(root_ico, client_ico)
    print(f"Copied to client: {client_png}, {client_ico}")
    
    # 4. Copy to build directories
    for b in ["build\\Debug", "build\\Release", "build\\bin"]:
        bpath = os.path.join(root_dir, b)
        if os.path.exists(bpath):
            shutil.copy2(root_png, os.path.join(bpath, "icon.png"))
            shutil.copy2(root_ico, os.path.join(bpath, "icon.ico"))
            print(f"Copied to {bpath}")
            
    print("All icons successfully deployed!")

if __name__ == "__main__":
    main()
