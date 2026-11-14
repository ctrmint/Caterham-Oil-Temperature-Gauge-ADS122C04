#!/usr/bin/env python3
"""Render docs/sender-curve.svg: sender resistance against temperature.

Inputs (paths relative to the repo root):
  docs/sender-reference-curves.csv  datasheet curves, reference only (never calibration)
  docs/sender-observations.csv      bench observations along a reference curve
  docs/sender-measurements.csv      measured points (optional rows)
  main/Config.h                     fault and alarm thresholds
  main/CalibrationData.h            the calibration table, drawn only when verified
Run it after changing any input. The output is deterministic, and
tests/test_sender_curve.py fails while the committed render is stale.
"""
import argparse
import csv
import math
import pathlib
import re
import sys
from xml.sax.saxutils import escape

OUTPUT='docs/sender-curve.svg'
REFERENCE='docs/sender-reference-curves.csv'
OBSERVATIONS='docs/sender-observations.csv'
MEASUREMENTS='docs/sender-measurements.csv'
CONFIG='main/Config.h'
CALIBRATION='main/CalibrationData.h'
CURVES=(  # reference column, legend label, colour
    ('bosch_0280130026_ohms','Bosch 0 280 130 026, datasheet','#1f6fb4'),
    ('tt6811_03_ohms','Caerbont TT6811-03 "Natural 205", published','#d9730d'),
)
THRESHOLDS=('kOpenResistanceOhms','kShortResistanceOhms','kWarmThresholdC','kAmberThresholdC','kRedThresholdC')

W,H=980,640
X0,X1,Y0,Y1=90,680,84,540          # plot area
TMIN,TMAX=-40,150                  # °C
DMIN,DMAX=1,5                      # log10 ohms: 10 Ω … 100 kΩ
LEGEND_X=710


def rows(path):
    with open(path,newline='',encoding='utf-8') as f:
        lines=[line for line in f if not line.lstrip().startswith('#')]
    reader=csv.DictReader(lines)
    return reader.fieldnames or [],[r for r in reader if any((v or '').strip() for v in r.values() if isinstance(v,str))]


def number(text,what):
    try:
        v=float(text)
    except (TypeError,ValueError):
        raise ValueError(f'{what}: expected a number, got {text!r}') from None
    if not math.isfinite(v):
        raise ValueError(f'{what}: must be finite')
    return v


def reference_curves(path):
    fields,data=rows(path)
    curves={}
    for column,_,_ in CURVES:
        if column not in fields:
            raise ValueError(f'{path}: missing column {column}')
        pts=[(number(r['temperature_c'],'temperature_c'),number(r[column],column))
             for r in data if (r.get(column) or '').strip()]
        for (a,ra),(b,rb) in zip(pts,pts[1:]):
            if not (b>a and 0<rb<ra):
                raise ValueError(f'{path}: {column} must rise in temperature and fall in resistance')
        if len(pts)<2:
            raise ValueError(f'{path}: {column} needs at least two points')
        curves[column]=pts
    return curves


def observations(path,curves):
    _,data=rows(path)
    out=[]
    for r in data:
        if r['curve'] not in curves:
            raise ValueError(f'{path}: unknown curve {r["curve"]!r}')
        lo,hi=number(r['from_c'],'from_c'),number(r['to_c'],'to_c')
        if not lo<hi:
            raise ValueError(f'{path}: from_c must be below to_c')
        out.append((r['curve'],lo,hi,r['date'].strip(),r['label'].strip()))
    return out


def measurements(path):
    if not pathlib.Path(path).exists():
        return []
    _,data=rows(path)
    pts=[]
    for r in data:
        t,ohms=number(r['temperature_c'],'temperature_c'),number(r['resistance_ohms'],'resistance_ohms')
        if ohms<=0:
            raise ValueError(f'{path}: resistance_ohms must be > 0')
        pts.append((t,ohms))
    return sorted(pts)


def thresholds(text):
    found={}
    for name in THRESHOLDS:
        m=re.search(r'constexpr\s+float\s+'+name+r'\s*=\s*(-?[0-9.]+)f?\s*;',text)
        if not m:
            raise ValueError(f'{CONFIG}: {name} not found')
        found[name]=float(m.group(1))
    return found


def calibration(text):
    m=re.search(r'kCalibrationVerified\s*=\s*(true|false)',text)
    if not m:
        raise ValueError(f'{CALIBRATION}: kCalibrationVerified not found')
    pts=[(float(t),float(r)) for t,r in re.findall(r'\{\s*(-?[0-9.]+)f\s*,\s*(-?[0-9.]+)f\s*\}',text)]
    return m.group(1)=='true',pts


def x(t): return X0+(t-TMIN)/(TMAX-TMIN)*(X1-X0)
def y(ohms): return Y1-(math.log10(ohms)-DMIN)/(DMAX-DMIN)*(Y1-Y0)
def inside(t,ohms): return TMIN<=t<=TMAX and 10**DMIN<=ohms<=10**DMAX
def f(v): return f'{v:.1f}'
def num(v): return f'{v:g}'.replace('-','−')  # typographic minus


def along(pts,t):
    """Resistance at t on a curve, interpolated in log space (as the curve is drawn)."""
    for (a,ra),(b,rb) in zip(pts,pts[1:]):
        if a<=t<=b:
            u=(t-a)/(b-a)
            return 10**(math.log10(ra)+u*(math.log10(rb)-math.log10(ra)))
    raise ValueError(f'{t} °C is outside the curve')


def wrap(text,width):
    lines,line=[],''
    for word in text.split():
        if line and len(line)+1+len(word)>width:
            lines.append(line); line=word
        else:
            line=f'{line} {word}' if line else word
    return lines+[line] if line else lines


def ohms_label(v):
    return f'{v/1000:g} kΩ' if v>=1000 else f'{v:g} Ω'


def text(s,tx,ty,size=12,anchor='start',fill='#222',weight=None,extra=''):
    w=f' font-weight="{weight}"' if weight else ''
    return f'<text x="{f(tx)}" y="{f(ty)}" font-size="{size}" text-anchor="{anchor}" fill="{fill}"{w}{extra}>{escape(s)}</text>'


def polyline(pts,colour,width=2.0,opacity=1.0,dash=None):
    d=f' stroke-dasharray="{dash}"' if dash else ''
    o=f' stroke-opacity="{opacity}"' if opacity<1 else ''
    coords=' '.join(f'{f(x(t))},{f(y(r))}' for t,r in pts)
    return f'<polyline points="{coords}" fill="none" stroke="{colour}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round"{o}{d}/>'


def render(root='.'):
    root=pathlib.Path(root)
    curves=reference_curves(root/REFERENCE)
    obs=observations(root/OBSERVATIONS,curves)
    meas=measurements(root/MEASUREMENTS)
    th=thresholds((root/CONFIG).read_text(encoding='utf-8'))
    verified,table=calibration((root/CALIBRATION).read_text(encoding='utf-8'))
    out=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Helvetica, Arial, sans-serif">',
         '<!-- Generated by scripts/plot_sender_curve.py. Do not edit by hand. -->',
         f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
         text('Oil-temperature sender: resistance against temperature',X0,34,18,weight='bold'),
         text('Reference curves are datasheet values for comparison only, never a calibration (CLAUDE.md invariant 1).',X0,56,12,fill='#555')]
    # Fault bands from main/Config.h.
    open_ohms,short_ohms=th['kOpenResistanceOhms'],th['kShortResistanceOhms']
    out.append(f'<rect x="{X0}" y="{Y0}" width="{X1-X0}" height="{f(y(open_ohms)-Y0)}" fill="#eceff3"/>')
    out.append(f'<rect x="{X0}" y="{f(y(short_ohms))}" width="{X1-X0}" height="{f(Y1-y(short_ohms))}" fill="#eceff3"/>')
    # Grid and axes.
    for d in range(DMIN,DMAX+1):
        for m in (1,2,5):
            v=m*10**d
            if v>10**DMAX: break
            out.append(f'<line x1="{X0}" y1="{f(y(v))}" x2="{X1}" y2="{f(y(v))}" stroke="{"#c9ced6" if m==1 else "#e3e6eb"}" stroke-width="1"/>')
            if m==1: out.append(text(ohms_label(v),X0-8,y(v)+4,12,'end',fill='#444'))
    for t in range(TMIN,TMAX+1,10):
        out.append(f'<line x1="{f(x(t))}" y1="{Y0}" x2="{f(x(t))}" y2="{Y1}" stroke="{"#c9ced6" if t%20==0 else "#e3e6eb"}" stroke-width="1"/>')
        if t%20==0: out.append(text(num(t),x(t),Y1+18,12,'middle',fill='#444'))
    out.append(f'<rect x="{X0}" y="{Y0}" width="{X1-X0}" height="{Y1-Y0}" fill="none" stroke="#888" stroke-width="1"/>')
    out.append(text('Temperature (°C)',(X0+X1)/2,Y1+40,13,'middle'))
    out.append(text('Resistance (log scale)',24,(Y0+Y1)/2,13,'middle',extra=f' transform="rotate(-90 24 {f((Y0+Y1)/2)})"'))
    out.append(text(f'SENSOR OPEN above {ohms_label(open_ohms)} (kOpenResistanceOhms)',X0+8,Y0+16,11,fill='#555'))
    out.append(text(f'SENSOR SHORT below {ohms_label(short_ohms)} (kShortResistanceOhms)',X0+8,Y1-8,11,fill='#555'))
    # Temperature thresholds.
    for name,label,colour,anchor,dy in (('kWarmThresholdC','OIL WARMING below','#5b7fa6','start',34),
                                        ('kAmberThresholdC','Amber','#d48a00','end',50),
                                        ('kRedThresholdC','Red','#c62828','end',66)):
        t=th[name]
        out.append(f'<line x1="{f(x(t))}" y1="{Y0}" x2="{f(x(t))}" y2="{Y1}" stroke="{colour}" stroke-width="1.5" stroke-dasharray="6 4"/>')
        dx=5 if anchor=='start' else -5
        out.append(text(f'{label} {num(t)} °C',x(t)+dx,Y0+dy,11,anchor,fill=colour))
    # Observations, under the curves they follow.
    for column,lo,hi,_,_ in obs:
        pts=[(t,along(curves[column],t)) for t in [lo]+[c for c,_ in curves[column] if lo<c<hi]+[hi]]
        out.append(polyline(pts,'#2e9e44',12,0.35))
    # Reference curves.
    for column,_,colour in CURVES:
        out.append(polyline(curves[column],colour,2.2))
        out.extend(f'<circle cx="{f(x(t))}" cy="{f(y(r))}" r="3" fill="{colour}"/>' for t,r in curves[column])
    # The calibration table, only when verified.
    if verified and len(table)>=2:
        out.append(polyline(table,'#111111',2.0))
        out.extend(f'<rect x="{f(x(t)-3)}" y="{f(y(r)-3)}" width="6" height="6" fill="#111111"/>' for t,r in table if inside(t,r))
    # Measured points.
    shown=[(t,r) for t,r in meas if inside(t,r)]
    out.extend(f'<circle cx="{f(x(t))}" cy="{f(y(r))}" r="4.5" fill="#c0392b" stroke="#ffffff" stroke-width="1"/>' for t,r in shown)
    # Legend.
    ly=Y0+6
    def entry(swatch,label,width=34):
        nonlocal ly
        out.append(swatch(ly))
        for i,line in enumerate(wrap(label,width)):
            out.append(text(line,LEGEND_X+34,ly+4+i*15,12))
        ly+=15*len(wrap(label,width))+12
    for column,label,colour in CURVES:
        span=f'{num(curves[column][0][0])}…{num(curves[column][-1][0])} °C'
        entry(lambda yy,c=colour:f'<line x1="{LEGEND_X}" y1="{yy}" x2="{LEGEND_X+24}" y2="{yy}" stroke="{c}" stroke-width="2.2"/><circle cx="{LEGEND_X+12}" cy="{yy}" r="3" fill="{c}"/>',
              f'{label} ({span})')
    for column,lo,hi,date,label in obs:
        entry(lambda yy:f'<line x1="{LEGEND_X}" y1="{yy}" x2="{LEGEND_X+24}" y2="{yy}" stroke="#2e9e44" stroke-width="12" stroke-opacity="0.35"/>',
              f'{label}, {num(lo)}–{num(hi)} °C ({date})')
    if verified and len(table)>=2:
        entry(lambda yy:f'<line x1="{LEGEND_X}" y1="{yy}" x2="{LEGEND_X+24}" y2="{yy}" stroke="#111111" stroke-width="2"/>',
              f'Verified calibration table, main/CalibrationData.h ({len(table)} points)')
    else:
        entry(lambda yy:f'<line x1="{LEGEND_X}" y1="{yy}" x2="{LEGEND_X+24}" y2="{yy}" stroke="#111111" stroke-width="2" stroke-dasharray="3 3"/>',
              'No verified calibration table in main/CalibrationData.h: the gauge shows ohms (fail-closed).')
    if meas:
        hidden=len(meas)-len(shown)
        note=f' ({hidden} outside the axes, not shown)' if hidden else ''
        entry(lambda yy:f'<circle cx="{LEGEND_X+12}" cy="{yy}" r="4.5" fill="#c0392b"/>',
              f'Measured points, docs/sender-measurements.csv ({len(meas)}){note}')
    else:
        entry(lambda yy:f'<circle cx="{LEGEND_X+12}" cy="{yy}" r="4.5" fill="none" stroke="#c0392b" stroke-width="1.5"/>',
              'No measured points in docs/sender-measurements.csv yet.')
    entry(lambda yy:f'<rect x="{LEGEND_X}" y="{yy-7}" width="24" height="14" fill="#eceff3" stroke="#c9ced6"/>',
          'Firmware fault bands (main/Config.h)')
    out.append(text('Generated by scripts/plot_sender_curve.py from docs/sender-*.csv, main/Config.h and main/CalibrationData.h.',X0,H-30,11,fill='#555'))
    out.append(text('Do not edit by hand: regenerate after changing any input. A host test fails while this file is stale.',X0,H-14,11,fill='#555'))
    out.append('</svg>')
    return '\n'.join(out)+'\n'


def main():
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--check',action='store_true',help='exit 1 if the committed render is stale, without writing')
    a=p.parse_args()
    root=pathlib.Path(__file__).resolve().parent.parent
    svg=render(root)
    target=root/OUTPUT
    if a.check:
        if not target.exists() or target.read_text(encoding='utf-8')!=svg:
            print(f'{OUTPUT} is stale: run python3 scripts/plot_sender_curve.py',file=sys.stderr)
            return 1
        print(f'{OUTPUT} is current')
        return 0
    target.write_text(svg,encoding='utf-8')
    print('Wrote',OUTPUT)
    return 0


if __name__=='__main__':
    sys.exit(main())
