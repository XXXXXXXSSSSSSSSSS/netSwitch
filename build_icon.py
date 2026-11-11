# Rebuild the existing geometric app icon at exact Windows DPI sizes.
from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGBA',(1024,1024),(0,0,0,0))
d=ImageDraw.Draw(im)
d.rounded_rectangle((34,34,990,990),radius=245,fill='#1166dc',outline='#86c5ff',width=18)
for r in (302,208): d.arc((512-r,435-r,512+r,435+r),220,320,fill='white',width=48)
d.ellipse((474,396,550,472),fill='white')
d.rounded_rectangle((365,620,659,789),radius=34,fill='white')
d.rectangle((407,589,617,650),fill='white')
for x in (431,489,547):d.rounded_rectangle((x,649,x+36,711),radius=8,fill='#1166dc')
d.rectangle((474,786,550,848),fill='white')
sizes=[16,20,24,28,32,40,48,56,64,72,96,128,192,256]
im.save(Path(__file__).with_name('app.ico'),sizes=[(n,n) for n in sizes])
