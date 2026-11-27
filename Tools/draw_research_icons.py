"""Draw the Vehicle Engineering MAM pictograms. Requires Pillow."""
from pathlib import Path
from math import cos, sin, radians
from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[1] / "Resources" / "Research"
OUT.mkdir(parents=True, exist_ok=True)
S = 4
INK = "#10212a"
PAPER = "#f5f4df"
PALETTES = dict(clock="#ffb64c", tractor="#e6a340", explorer="#5ad8b4",
                truck="#7ebbee", drift="#e990bc", fleet="#f6cb69")


def render(name):
    family, _, detail = name.partition("_")
    accent = PALETTES[family]
    im = Image.new("RGBA", (96*S, 96*S))
    d = ImageDraw.Draw(im)

    def pts(p):
        return [(round(x*S), round(y*S)) for x, y in p]

    def poly(p, c):
        d.polygon(pts(p), fill=c)

    def line(p, c=PAPER, w=3):
        d.line(pts(p), fill=c, width=round(w*S), joint="curve")

    def box(x, y, xx, yy, c):
        poly([(x,y),(xx,y),(xx,yy),(x,yy)], c)

    def disc(x, y, r, c):
        d.ellipse((int((x-r)*S),int((y-r)*S),int((x+r)*S),int((y+r)*S)), fill=c)

    def wheel(x, y, r):
        disc(x, y, r, INK)
        disc(x, y, r-1.6, PAPER)
        disc(x, y, r*.52, INK)
        disc(x, y, r*.21, accent)

    def plate(m):
        return [(m+10,m),(94-m,m),(94-m,79-m),(81-m,94-m),
                (m,94-m),(m,10+m)]

    poly(plate(2), accent)
    poly(plate(5), INK)
    poly([(8,18),(18,8),(84,8),(84,12),(16,12),(8,20)], "#354952")
    if family in ("tractor","truck","explorer"):
        if family == "tractor":
            poly([(12,42),(18,40),(21,23),(42,23),(48,38),(68,38),
                  (76,43),(76,51),(65,53),(22,53),(12,49)], accent)
            poly([(25,27),(38,27),(42,37),(24,37)], INK)
            line([(58,36),(58,26),(62,25)], PAPER, 3)
            line([(17,22),(44,22)], PAPER, 3)
            wheel(29,51,11); wheel(66,53,8)
        elif family == "explorer":
            line([(16,43),(29,38),(37,25),(52,24),(64,38),(75,41)], PAPER, 3.3)
            line([(32,38),(43,38),(50,26)], PAPER, 2.5)
            poly([(12,44),(28,38),(47,40),(63,35),(80,42),(77,49),
                  (60,50),(50,54),(24,51),(13,48)], accent)
            wheel(25,51,10); wheel(69,51,10)
            line([(40,22),(50,22)], accent, 3)
        else:
            poly([(10,24),(50,24),(50,42),(10,42)], accent)
            for x in (17,25,33,41):
                line([(x,28),(x,38)], INK, 2)
            poly([(52,28),(65,28),(78,40),(80,49),(73,51),(17,51),(12,46),
                  (52,45)], PAPER)
            poly([(56,31),(64,31),(71,39),(56,39)], INK)
            wheel(22,51,8); wheel(40,51,8); wheel(68,51,9)
        line([(14,65),(80,65)], "#415158", 1)
        kind = detail.rstrip("123")
        rank = int(detail[-1]) if detail and detail[-1].isdigit() else 0
        if kind == "grip":
            for x in (19,28,37): line([(x,73),(x+4,77),(x,81)], accent,3)
        elif kind == "aero":
            for x in (22,33): line([(x,71),(x,80)], PAPER,2.5)
            line([(17,77),(22,82),(27,77)],accent,3)
            line([(28,77),(33,82),(38,77)],accent,3)
        elif kind == "rocket":
            poly([(25,73),(38,73),(45,77),(38,81),(25,81)], PAPER)
            poly([(24,74),(13,77),(24,80)], accent)
        elif kind == "brake":
            disc(28,77,8,PAPER); disc(28,77,4,INK)
            poly([(29,68),(37,69),(41,75),(37,78),(32,73)],accent)
        elif kind == "stability":
            line([(16,78),(23,78),(27,72),(32,82),(36,76),(43,76)], accent,3)
        elif kind == "ballast":
            poly([(20,73),(36,73),(40,82),(16,82)],accent)
            line([(25,72),(25,69),(31,69),(31,72)],PAPER,2)
        elif kind == "spring":
            line([(22,69),(33,71),(22,74),(33,77),(22,80),(33,82)],accent,3)
        elif kind == "steer":
            disc(28,77,9,PAPER); disc(28,77,6,INK)
            line([(21,75),(28,78),(35,75)],accent,2)
            line([(28,78),(28,83)],accent,2)
        elif kind == "drift":
            line([(16,82),(24,80),(30,74),(34,70)],accent,3)
            line([(26,84),(34,82),(40,76)],PAPER,2)
        elif kind in ("air", "thruster"):
            line([(16,80),(27,70),(38,80)],accent,3)
            line([(27,70),(27,83)],PAPER,2)
            if kind == "thruster":
                line([(42,74),(48,78),(42,82)],accent,3)
        elif kind == "land":
            line([(17,82),(40,82)],PAPER,2)
            line([(28,69),(28,78)],accent,3)
            line([(22,74),(28,79),(34,74)],accent,3)
        elif kind == "exit":
            line([(16,80),(26,80),(34,74),(41,74)],accent,3)
            line([(37,70),(42,74),(37,78)],PAPER,2)
        else:
            for x in (21,30,39):
                line([(x,72),(x-4,82)],accent,3)
        if rank:
            for i in range(3): box(55+i*8,74,60+i*8,81,accent if i<rank else "#374951")
        else:
            line([(59,78),(75,78)], accent,2)
    elif family == "fleet":
        poly([(48,18),(58,32),(66,44),(67,50),(64,58),(58,64),
              (50,67),(42,66),(34,62),(29,55),(29,47),(35,36)], accent)
        line([(36,48),(37,54),(42,58)], PAPER, 3)
        for i in range(4):
            box(25+i*12,75,33+i*12,82,accent if i<int(detail) else "#374951")
    elif family == "clock":
        for a in range(140,401,5):
            xy=lambda r,t:(48+r*cos(radians(t)),51+r*sin(radians(t)))
            line([xy(31,a),xy(31,a+5)],PAPER,4)
        for a in (150,190,230,270,310,350):
            line([xy(24,a),xy(29,a)],accent,3)
        poly([(36,57),(43,46),(69,32),(54,54),(48,60)],accent)
        disc(46,54,5,PAPER); disc(46,54,2,INK)
        for i in range(3): box(33+i*11,75,40+i*11,81,accent)
    else:
        # Curved tyre marks follow an angled rear axle.
        line([(15,71),(26,68),(36,63),(43,54)], accent,4)
        line([(17,82),(36,75),(48,66),(53,57)], PAPER,3)
        poly([(43,47),(55,24),(62,20),(75,27),(77,34),(63,59),
              (57,63),(42,55)],accent)
        poly([(48,44),(56,30),(69,36),(62,49)],INK)
        line([(45,50),(59,57)],PAPER,3)
        line([(31,39),(35,31),(40,27)],PAPER,2)
    im.resize((128,128), Image.Resampling.LANCZOS).save(OUT / (name+".png"), optimize=True)


if __name__ == "__main__":
    for icon in ["clock","tractor","tractor_grip1","tractor_grip2","tractor_grip3","tractor_aero1","tractor_aero2","tractor_aero3","tractor_brake1","tractor_brake2","tractor_brake3","tractor_stability","tractor_rocket","explorer","explorer_ballast","explorer_spring","explorer_brake","explorer_stability","truck","truck_grip","truck_brake","truck_stability","drift"]:
        render(icon)
    for icon in ["fleet_1","fleet_2","fleet_3","fleet_4","explorer_steer1","explorer_steer2","explorer_steer3"]:
        render(icon)
    for icon in ["explorer_drift1","explorer_drift2","explorer_exit","explorer_air1","explorer_air2","explorer_air3","explorer_thruster","explorer_land1","explorer_land2"]:
        render(icon)
    for icon in ["tractor_air1","tractor_air2","tractor_air3"]:
        render(icon)
