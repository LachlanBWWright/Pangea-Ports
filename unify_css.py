import os
import glob
import re

def update_css():
    target_files = [
        "games/MightyMike-Android/docs/index.html",
        "games/Bugdom2-Android/docs/index.html",
        "games/CroMagRally-Android/docs/index.html",
        "games/Bugdom-android/docs/index.html",
        "games/BillyFrontier-Android/docs/index.html",
        "games/Nanosaur-android/docs/index.html",
        "games/Nanosaur2-Android/docs/index.html",
        "games/OttoMatic-Android/docs/index.html",
    ]

    # Desired theme block
    theme_block = """    :root {
      color-scheme: dark;
      --bg: #09111f;
      --panel: #13203a;
      --accent: #8cd0ff;
      --accent-2: #ffd280;
      --text: #edf5ff;
      --muted: #c2d4e7;
      --border: #27456f;
    }"""

    for filepath in target_files:
        if not os.path.exists(filepath):
            continue
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Regex to find :root { ... } and replace it
        new_content = re.sub(r':root\s*\{[^}]+\}', theme_block, content, flags=re.MULTILINE)

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated {filepath}")

if __name__ == '__main__':
    update_css()
