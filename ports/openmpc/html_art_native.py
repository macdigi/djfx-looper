#!/usr/bin/env python3
"""Build the public native skin from pinned SVG jobs using librsvg.

This produces build artwork only. It opens no app, audio device or user browser.
Fontconfig and its cache are private to the current output directory.
The html_art filename prefix selects the pinned tool's SVG-job protocol; this
adapter renders that protocol with librsvg and never launches a browser.
"""
from pathlib import Path
import io
import os
import re
import subprocess
import sys
from html import escape
from PIL import Image

tools = Path(os.environ['MPC_VST_TOOLS'])
sys.path.insert(0, str(tools))
from html_art import Art, DEFS

art = Art()
for raw in sys.stdin:
    line = raw.rstrip('\r\n')
    if line:
        art.run(line)
if not art.jobs:
    raise SystemExit(0)

page = art.page()
css = '\n'.join(re.findall(r'<style[^>]*>(.*?)</style>', page, re.S))
css += '\n' + (tools / 'html_art/default.css').read_text()
css += '\n' + '\n'.join(Path(style).read_text() for style in art.css)
variables = dict(re.findall(r'--([\w-]+)\s*:\s*([^;}]+)', css))
# Defaults for button/segment selectors in the shared stylesheet. This port's
# knob-only layout does not instantiate those per-element colour overrides.
for key, theme in [('fill','panel'), ('ink','ink'), ('border','line')]:
    variables.setdefault(key, '#' + art.theme[theme])
def resolve(text):
    for _ in range(8):
        changed = re.sub(r'var\(--([\w-]+)\)', lambda match: variables[match[1]], text)
        if changed == text:
            break
        text = changed
    return re.sub(r'calc\(\s*([.\d]+)\s*\*\s*([.\d]+)\s*\)',
                  lambda m: str(float(m[1]) * float(m[2])), text)
css = resolve(css)
defs = resolve(DEFS)
assert 'var(' not in css + defs
output = Path(art.jobs[0][2] if art.jobs[0][0] == 'crop' else art.jobs[0][1]).parent
config = output / 'fontconfig.xml'
font_dir = Path(__file__).resolve().parent / 'assets'
config.write_text('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                  '<dir>' + escape(str(font_dir)) + '</dir><dir>/System/Library/Fonts</dir>'
                  '<cachedir>' + escape(str(output / 'font-cache')) + '</cachedir></fontconfig>')
environment = {**os.environ, 'FONTCONFIG_FILE': str(config)}

def render(body, width, height):
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d">'
           '<style>%s</style>%s%s%s</svg>') % (width, height, css, defs, art.image_defs(), body)
    result = subprocess.run(['rsvg-convert', '--format=png'], input=svg.encode(),
                            env=environment, capture_output=True, check=True)
    return Image.open(io.BytesIO(result.stdout)).convert('RGBA')

last = None
image = None
for job in art.jobs:
    if job[0] == 'crop':
        _, body, path, x, y, width, height, alpha = job
        if body != last:
            image = render(body, 1280, 800)
            last = body
        result = Image.new('RGBA' if alpha else 'RGB', (width, height), (0,0,0,0) if alpha else (0,0,0))
        left, top, right, bottom = max(0,x), max(0,y), min(1280,x+width), min(800,y+height)
        if right > left and bottom > top:
            result.paste(image.crop((left, top, right, bottom)).convert(result.mode), (left-x, top-y))
        result.save(path, 'PNG' if alpha else 'PPM')
    else:
        _, path, width, height, frames, parts = job
        result = Image.new('RGBA', (width, height*frames))
        for start in range(0, frames, max(1,8000//height)):
            chunk = parts[start:start+max(1,8000//height)]
            body = ''.join('<g transform="translate(0 %d)">%s</g>' % (i*height, frame)
                           for i,frame in enumerate(chunk))
            result.paste(render(body, width, height*len(chunk)), (0, start*height))
        result.save(path, 'PNG')
print('Rendered %d native skin jobs with librsvg' % len(art.jobs), file=sys.stderr)
