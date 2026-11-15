"""Review plot in board coordinates (origin at the board centre): courtyards, pads (net-labelled),
tracks, holes, outline, 1 mm grid."""
import os, sys, math, pcbnew
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import layout as L  # noqa: E402
b = pcbnew.LoadBoard(sys.argv[1]); out = sys.argv[2]
S = 14.0  # px per mm
X0, X1, Y0, Y1 = -L.WIDTH / 2 - 2, L.WIDTH / 2 + 2, -L.HEIGHT / 2 - 2, L.HEIGHT / 2 + 2
def T(v): return ((pcbnew.ToMM(v.x) - 100 - X0) * S, (Y1 - (100 - pcbnew.ToMM(v.y))) * S)
def TP(x, y): return ((x - X0) * S, (Y1 - y) * S)
W, H = (X1 - X0) * S, (Y1 - Y0) * S
o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.0f}" height="{H:.0f}" font-family="monospace">',
     f'<rect width="{W}" height="{H}" fill="white"/>']
for gx in range(int(X0), int(X1) + 1):
    a, _ = TP(gx, 0); o.append(f'<line x1="{a}" y1="0" x2="{a}" y2="{H}" stroke="{"#bbb" if gx % 5 == 0 else "#eee"}" stroke-width="1"/>')
    if gx % 5 == 0: o.append(f'<text x="{a+2}" y="12" font-size="11" fill="#888">{gx}</text>')
for gy in range(int(Y0), int(Y1) + 1):
    _, c = TP(0, gy); o.append(f'<line x1="0" y1="{c}" x2="{W}" y2="{c}" stroke="{"#bbb" if gy % 5 == 0 else "#eee"}" stroke-width="1"/>')
    if gy % 5 == 0: o.append(f'<text x="2" y="{c-2}" font-size="11" fill="#888">{gy}</text>')
for d in b.GetDrawings():
    if d.GetLayer() == pcbnew.Edge_Cuts:
        if d.GetShape() == pcbnew.SHAPE_T_ARC:
            s, m, e = T(d.GetStart()), T(d.GetArcMid()), T(d.GetEnd())
            r = pcbnew.ToMM(d.GetRadius()) * S
            o.append(f'<path d="M{s[0]},{s[1]} A{r},{r} 0 0 1 {e[0]},{e[1]}" fill="none" stroke="black" stroke-width="2"/>')
        else:
            s, e = T(d.GetStart()), T(d.GetEnd()); o.append(f'<line x1="{s[0]}" y1="{s[1]}" x2="{e[0]}" y2="{e[1]}" stroke="black" stroke-width="2"/>')
col = {}
pal = ['#e6194b','#3cb44b','#4363d8','#f58231','#911eb4','#42d4f4','#f032e6','#9a6324','#800000','#808000','#000075','#469990','#dcbeff','#aaffc3']
def nc(n):
    if n in ('', None): return '#999'
    if n == 'GND': return '#666'
    col.setdefault(n, pal[len(col) % len(pal)]); return col[n]
for t in b.GetTracks():
    n = t.GetNetname()
    if t.GetClass() == 'PCB_VIA':
        p = T(t.GetPosition()); o.append(f'<circle cx="{p[0]}" cy="{p[1]}" r="{pcbnew.ToMM(t.GetWidth(pcbnew.F_Cu))/2*S}" fill="{nc(n)}" stroke="black"/>')
    else:
        s, e = T(t.GetStart()), T(t.GetEnd()); dash = ' stroke-dasharray="5 3" opacity="0.6"' if t.GetLayer() == pcbnew.B_Cu else ' opacity="0.8"'
        o.append(f'<line x1="{s[0]}" y1="{s[1]}" x2="{e[0]}" y2="{e[1]}" stroke="{nc(n)}" stroke-width="{pcbnew.ToMM(t.GetWidth())*S}" stroke-linecap="round"{dash}/>')
for fp in b.GetFootprints():
    cy = fp.GetCourtyard(pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd)
    if cy.OutlineCount():
        pts = [T(cy.Outline(0).CPoint(i)) for i in range(cy.Outline(0).PointCount())]
        o.append('<polygon points="' + ' '.join(f'{x},{y}' for x, y in pts) + f'" fill="none" stroke="{"#c0c" if not fp.IsFlipped() else "#0aa"}" stroke-width="1.5"/>')
    for p in fp.Pads():
        pos = T(p.GetPosition()); sz = p.GetSize(pcbnew.F_Cu); ang = p.GetOrientationDegrees()
        w, h = pcbnew.ToMM(sz.x) * S, pcbnew.ToMM(sz.y) * S
        n = p.GetNetname(); fill = nc(n)
        if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
            o.append(f'<circle cx="{pos[0]}" cy="{pos[1]}" r="{pcbnew.ToMM(p.GetDrillSize().x)/2*S}" fill="black"/>'); continue
        o.append(f'<rect x="{-w/2}" y="{-h/2}" width="{w}" height="{h}" fill="{fill}" opacity="0.75" transform="translate({pos[0]},{pos[1]}) rotate({-ang})"/>')
        if p.GetDrillSize().x: o.append(f'<circle cx="{pos[0]}" cy="{pos[1]}" r="{pcbnew.ToMM(p.GetDrillSize().x)/2*S}" fill="white"/>')
        lbl = n.lstrip('/')[:6] if n and not n.startswith('unconn') else ''
        o.append(f'<text x="{pos[0]}" y="{pos[1]+3}" font-size="8" text-anchor="middle" fill="black">{p.GetNumber()}</text>')
        if lbl: o.append(f'<text x="{pos[0]}" y="{pos[1]+11}" font-size="7" text-anchor="middle" fill="#000">{lbl}</text>')
    c = T(fp.GetPosition())
    o.append(f'<text x="{c[0]}" y="{c[1]-4}" font-size="13" font-weight="bold" text-anchor="middle" fill="#a0a">{fp.GetReference()}</text>')
o.append('</svg>')
open(out, 'w').write('\n'.join(o))
