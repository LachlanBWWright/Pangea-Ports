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
    ]

    for filepath in target_files:
        if not os.path.exists(filepath):
            continue
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Update variable names if they're different
        content = re.sub(r'var\(--card\)', 'var(--panel)', content)
        content = re.sub(r'var\(--dim\)', 'var(--muted)', content)

        # Standardize canvas container layout
        if "embedded-shell" not in content:
            style_append = """
    body.embedded-shell {
      background: transparent;
    }
    body.embedded-shell header {
      display: none;
    }
    body.embedded-shell #game-wrap {
      padding: 0;
      height: 100dvh;
      display: flex;
      flex-direction: column;
      align-items: stretch;
      justify-content: center;
    }
    body.embedded-shell #canvas {
      display: block;
      width: 100%;
      height: 100%;
      max-width: none;
      border-radius: 0;
    }
"""
            content = content.replace("</style>", style_append + "</style>")

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Updated {filepath}")

if __name__ == '__main__':
    update_css()
