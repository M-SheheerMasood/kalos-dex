"""Square Minecraft clock preview (same maths as firmware src/screens/clock_screen.cpp).
usage: clock_preview.py [out.png]  -> 6 times of day side by side"""
import math, sys
from PIL import Image
TEX=4; W,H=60,80; CX,CY=W/2,H/2; HORIZON=H//2; BORDER=4; NUM_R=21.5; ORBIT=31; DIM=0.33
def mix(a,b,t): t=max(0,min(1,t)); return tuple(a[i]+(b[i]-a[i])*t for i in range(3))
def h32(x,y):
    h=(x*374761393+y*668265263)&0xffffffff; h=((h^(h>>13))*1274126177)&0xffffffff; return h^(h>>16)
DAY,SUNSET,NIGHT=(120,167,255),(255,132,60),(12,14,44)
CLOUDS=[(-50,34,8,2),(-25,16,6,2),(5,40,9,3),(20,27,5,2),(48,36,7,2),(-8,46,6,2),(35,12,4,1)]
GLYPH={'0':['111','101','101','101','111'],'1':['010','110','010','010','111'],'2':['111','001','111','100','111'],
'3':['111','001','111','001','111'],'4':['101','101','111','001','001'],'5':['111','100','111','001','111'],
'6':['111','100','111','101','111'],'7':['111','001','001','010','010'],'8':['111','101','111','101','111'],'9':['111','101','111','001','111']}
def sword(n):  # rows from the pommel (u=-2) to the tip, 5 columns across
    return ['.opo.','.oho.','.oho.','.oho.','.oho.','ogggo']+['oLMSo']*(n-8)+['.oMo.','..o..']
IRON={'L':(255,255,255),'M':(205,205,210),'S':(130,130,140)}
DIAMOND={'L':(200,255,250),'M':(80,215,205),'S':(30,140,130)}
COMMON={'o':(20,20,25),'h':(120,85,45),'p':(85,62,35),'g':(70,52,32)}
def frame(hh,mm,ss):
    hours=hh+mm/60+ss/3600; rot=(hours-12)*15
    up=math.cos(math.radians(rot)); dusk=max(0,1-abs(up)/0.4); side=math.sin(math.radians(rot))
    t=[[None]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            dx,dy=x+.5-CX,y+.5-CY
            phi=(math.degrees(math.atan2(dx,-dy))-rot+180)%360-180; a=abs(phi)
            s=DAY if a<87 else NIGHT if a>93 else (DAY if (x+y)&1 else NIGHT)
            if y<HORIZON:
                vf=max(0,min(1,1-(HORIZON-y-1)/12)); hf=max(0,min(1,.5+.5*(dx/CX)*(1 if side>=0 else -1)))
                g=math.floor(dusk*vf*hf*4)/4; s=mix(s,SUNSET,g*.9)
            t[y][x]=s
    def put(x,y,c):
        if 0<=x<W and 0<=y<H: t[y][x]=c
    def at(ang,r):
        a=math.radians(ang+rot); return CX+r*math.sin(a), CY-r*math.cos(a)
    def block(cx,cy,w,h,c):
        x0=round(cx-w/2); y0=round(cy-h/2)
        for yy in range(y0,y0+h):
            for xx in range(x0,x0+w): put(xx,yy,c)
    for i in range(110):
        h=h32(i,77); px,py=at(100+(h%160), 3+(h>>8)%48); put(int(px),int(py),(255,255,255) if i%3 else (190,200,255))
    for a,r,w,h in CLOUDS:
        px,py=at(a,r); block(px,py,w,h,(245,245,245)); block(px+1,py+1,w-2,1,(215,220,235))
    px,py=at(0,ORBIT); block(px,py,8,8,(255,214,64)); block(px,py,6,6,(255,238,120)); block(px,py,2,2,(255,252,215))
    px,py=at(180,ORBIT); block(px,py,7,7,(222,224,205))
    for ox,oy,cc in ((-2,-1,(165,168,160)),(1,1,(165,168,160)),(2,-2,(185,187,178))): put(round(px+ox),round(py+oy),cc)
    # below the horizon: the same turning sky, very dim (dithered edge row)
    for y in range(HORIZON-1,H):
        for x in range(W):
            if y==HORIZON-1 and (x&1): continue
            if y==HORIZON-1: k=0.66
            else: k=DIM
            t[y][x]=tuple(v*k for v in t[y][x])
    for y in range(H):
        for x in range(W): t[y][x]=tuple(int(v) for v in t[y][x])
    # gold border
    for y in range(H):
        for x in range(W):
            d=min(x,y,W-1-x,H-1-y)
            if d>=BORDER: continue
            if (x in (0,W-1)) and (y in (0,H-1)): t[y][x]=(16,22,40); continue
            tl=(x<=y and x<=W-1-x and x<H-1-y) or (y<x and y<W-1-x and y<H-1-y)
            if d==0: c=(120,80,10)
            elif d==BORDER-1: c=(95,62,8)
            elif d==1: c=(255,236,140) if tl else (190,135,25)
            else: c=(240,200,70) if tl else (215,160,35)
            if d==2 and h32(x,y)%9==0: c=(255,250,200) if tl else (235,190,60)
            t[y][x]=c
    # numbers on a circle: gold with a full dark outline
    def text(s,cx,cy):
        w=len(s)*4-1; x0=round(cx-w/2); y0=round(cy-2.5)
        pts=[(x0+k*4+gx,y0+gy) for k,ch in enumerate(s) for gy,row in enumerate(GLYPH[ch]) for gx,b in enumerate(row) if b=='1']
        for X,Y in pts:
            for ox in (-1,0,1):
                for oy in (-1,0,1): put(X+ox,Y+oy,(25,18,8))
        for X,Y in pts: put(X,Y,(255,215,70) if (Y-y0)<2 else (230,170,40))
    for i in range(1,13):
        a=math.radians(i*30); text(str(i),CX+math.sin(a)*NUM_R,CY-math.cos(a)*NUM_R)
    # sword hands: the sprite is turned by sampling it for every Minecraft pixel (stays blocky)
    def draw_sword(ang,n,pal):
        spr=sword(n); a=math.radians(ang); ux,uy=math.sin(a),-math.cos(a)
        for y in range(H):
            for x in range(W):
                px,py=x+.5-CX,y+.5-CY; u=px*ux+py*uy; v=px*-uy+py*ux
                r=math.floor(u+2); c=round(v)+2
                if 0<=r<len(spr) and 0<=c<5:
                    ch=spr[r][c]
                    if ch!='.': t[y][x]=pal.get(ch,COMMON.get(ch))
    draw_sword((hh%12+mm/60+ss/3600)*30,17,IRON)
    draw_sword((mm+ss/60)*6,25,DIAMOND)
    a=math.radians(ss*6); ux,uy=math.sin(a),-math.cos(a)
    for y in range(H):
        for x in range(W):
            px,py=x+.5-CX,y+.5-CY; s=max(-5,min(24,px*ux+py*uy))
            if (px-s*ux)**2+(py-s*uy)**2<=0.25: t[y][x]=(230,30,30)
    for y in (39,40):
        for x in (29,30): t[y][x]=(230,30,30)
    im=Image.new('RGB',(W,H)); im.putdata([c for row in t for c in row])
    return im.resize((W*TEX,H*TEX),Image.NEAREST)
if __name__=='__main__':
    times=[(6,10,0),(10,10,30),(12,40,15),(17,50,40),(22,6,50),(0,30,0)]
    sheet=Image.new('RGB',(3*250,2*330),(0,0,0))
    for i,tm in enumerate(times): sheet.paste(frame(*tm),((i%3)*250+5,(i//3)*330+5))
    sheet.save(sys.argv[1] if len(sys.argv)>1 else 'clock_preview.png')
