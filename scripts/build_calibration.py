#!/usr/bin/env python3
"""Validate measured temperature/resistance CSV and produce firmware calibration header."""
import argparse
import csv
import pathlib
import math
import sys


MIN_POINTS=5  # docs/CALIBRATION.md: at least five, preferably 7-10


def load(path):
    with open(path, newline='', encoding='utf-8') as f:
        # Lines starting with '#' are comments (see examples/measurements.example.csv).
        lines=[line for line in f if not line.lstrip().startswith('#')]
    reader=csv.DictReader(lines)
    if not {'temperature_c','resistance_ohms'}.issubset(reader.fieldnames or []):
        raise ValueError('CSV must include temperature_c,resistance_ohms')
    points=[]
    for n,row in enumerate(reader,start=2):
        if not any(v.strip() for v in row.values() if isinstance(v,str)):
            continue
        try:
            t,r=float(row['temperature_c']),float(row['resistance_ohms'])
        except (TypeError,ValueError):
            raise ValueError(f'Data row {n}: expected numeric temperature_c,resistance_ohms') from None
        if not (math.isfinite(t) and math.isfinite(r) and r>0):
            raise ValueError(f'Data row {n}: values must be finite, resistance > 0')
        points.append((t,r))
    points.sort()
    if len(points)<MIN_POINTS:
        raise ValueError(f'Minimum {MIN_POINTS} independently verified calibration points')
    for (a,ra),(b,rb) in zip(points,points[1:]):
        if not b>a or not rb<ra:
            raise ValueError('Expected unique increasing temperature, decreasing resistance')
    return points

def generate(points):
    lines=['#pragma once','// Generated ONLY from user-verified calibration measurements.',
           '// Do not use readings beyond these endpoints.',
           'inline constexpr bool kCalibrationVerified = true;',
           'inline constexpr gauge::CalibrationPoint kCalibrationPoints[] = {']
    lines.extend('  {%.3ff, %.3ff},'%(t,r) for t,r in points)
    lines.append('};')
    return '\n'.join(lines)+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('csv',type=pathlib.Path)
    p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('main/CalibrationData.h'))
    p.add_argument('--accept-verified-measurements',action='store_true',help='I personally verified sender calibration')
    a=p.parse_args()
    if not a.accept_verified_measurements:
        p.error('Refusing to enable temperature reporting without --accept-verified-measurements')
    pts=load(a.csv)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(generate(pts),encoding='utf-8')
    print('Wrote',a.output,'with',len(pts),'verified calibration points')

if __name__=='__main__':
    try:main()
    except (ValueError,OSError) as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(2)
