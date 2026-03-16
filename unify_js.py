import os
import re

def update_js():
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

    for filepath in target_files:
        if not os.path.exists(filepath):
            continue
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        if "_isEmbedded" not in content:
            js_append = """
    var _isEmbedded = new URLSearchParams(window.location.search).get('embed') === '1';
    if (_isEmbedded) {
      document.body.classList.add('embedded-shell');
    }
"""
            # find first <script> tag and append it there
            content = re.sub(r'<script>', '<script>' + js_append, content, count=1)

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Updated {filepath}")

if __name__ == '__main__':
    update_js()
