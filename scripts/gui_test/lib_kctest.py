"""
kernelCAD GUI test helper library.

Provides:
  KCTest        - one test session (screenshot, click, type, key)
  expect()      - assertions on screen state via image comparison or text presence

The test runs against a kernelCAD instance already launched on $DISPLAY by
gui_runner.sh.  The test directory is in $KERNELCAD_TEST_DIR.
"""
import os
import sys
import json
import time
import subprocess
from pathlib import Path

import mss
import mss.tools

TEST_DIR = Path(os.environ.get('KERNELCAD_TEST_DIR', '/tmp/kernelcad_gui_test/default'))
SCREENSHOT_DIR = TEST_DIR / 'screenshots'
SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)

class KCTest:
    def __init__(self, name='test'):
        self.name = name
        self.events = []
        self.screenshot_count = 0
        self.t0 = time.time()
        self._failures = []

    def now_ms(self):
        return int((time.time() - self.t0) * 1000)

    def event(self, kind, **kwargs):
        e = {'time_ms': self.now_ms(), 'kind': kind, **kwargs}
        self.events.append(e)
        print(f"  [{e['time_ms']:>6}ms] {kind}: {kwargs}")
        return e

    def screenshot(self, label='shot'):
        """Capture a full-screen PNG via mss."""
        self.screenshot_count += 1
        path = SCREENSHOT_DIR / f'{self.screenshot_count:03d}_{label}.png'
        with mss.mss() as sct:
            mon = sct.monitors[1]
            img = sct.grab(mon)
            mss.tools.to_png(img.rgb, img.size, output=str(path))
        self.event('screenshot', path=str(path), label=label)
        return path

    def click(self, x, y, button='left', label=''):
        """Click at absolute screen position via xdotool."""
        btn_map = {'left': '1', 'middle': '2', 'right': '3'}
        subprocess.run(['xdotool', 'mousemove', str(x), str(y), 'click', btn_map[button]],
                       check=False)
        self.event('click', x=x, y=y, button=button, label=label)
        time.sleep(0.15)

    def move(self, x, y, label=''):
        subprocess.run(['xdotool', 'mousemove', str(x), str(y)], check=False)
        self.event('move', x=x, y=y, label=label)
        time.sleep(0.05)

    def key(self, k, label=''):
        """Send a key (xdotool key syntax: 's', 'Escape', 'ctrl+s', etc.)"""
        subprocess.run(['xdotool', 'key', '--clearmodifiers', k], check=False)
        self.event('key', key=k, label=label)
        time.sleep(0.15)

    def typetext(self, text, label=''):
        subprocess.run(['xdotool', 'type', '--delay', '40', text], check=False)
        self.event('type', text=text, label=label)
        time.sleep(0.15)

    def wait(self, seconds, label=''):
        if label:
            self.event('wait', seconds=seconds, label=label)
        time.sleep(seconds)

    def find_window(self, name='kernelCAD'):
        """Return the X window ID of the first matching window."""
        try:
            wid = subprocess.check_output(['xdotool', 'search', '--name', name],
                                          text=True, stderr=subprocess.DEVNULL).strip().split('\n')[0]
            return wid
        except subprocess.CalledProcessError:
            return None

    def get_window_geometry(self):
        """Return (x, y, w, h) of the kernelCAD window."""
        wid = self.find_window('kernelCAD')
        if not wid:
            return None
        try:
            out = subprocess.check_output(['xdotool', 'getwindowgeometry', wid], text=True)
            x = int([l for l in out.split('\n') if 'Position' in l][0].split(':')[1].strip().split(',')[0])
            y = int([l for l in out.split('\n') if 'Position' in l][0].split(':')[1].strip().split(',')[1].split(' ')[0])
            wh = [l for l in out.split('\n') if 'Geometry' in l][0].split(':')[1].strip().split('x')
            return (x, y, int(wh[0]), int(wh[1]))
        except Exception:
            return None

    def expect_window_exists(self, name='kernelCAD'):
        wid = self.find_window(name)
        ok = wid is not None
        self._record_check(f"window '{name}' exists", ok, detail=f"wid={wid}")
        return ok

    def expect_pixel_changed(self, before_path, after_path, min_pct=0.1):
        """Compare two screenshots; pass if more than min_pct of pixels differ."""
        try:
            from PIL import Image
            import numpy as np
            a = np.array(Image.open(before_path).convert('RGB'))
            b = np.array(Image.open(after_path).convert('RGB'))
            if a.shape != b.shape:
                self._record_check("pixel diff", False, detail="shape mismatch")
                return False
            diff = np.any(np.abs(a.astype(int) - b.astype(int)) > 5, axis=-1)
            pct = (diff.sum() / diff.size) * 100
            ok = bool(pct >= min_pct)
            self._record_check(f"pixels changed >={min_pct}%",
                               ok, detail=f"actual={float(pct):.3f}%")
            return ok
        except Exception as e:
            self._record_check("pixel diff", False, detail=str(e))
            return False

    def _record_check(self, name, ok, detail=''):
        status = 'PASS' if ok else 'FAIL'
        line = f"  [{status}] {name}" + (f" ({detail})" if detail else '')
        print(line)
        self.events.append({'time_ms': self.now_ms(), 'kind': 'check',
                            'name': name, 'pass': ok, 'detail': detail})
        if not ok:
            self._failures.append(name)

    def finish(self):
        # Save events
        log_path = TEST_DIR / 'events.json'
        with open(log_path, 'w') as f:
            json.dump({
                'name': self.name,
                'duration_ms': self.now_ms(),
                'failures': self._failures,
                'events': self.events,
            }, f, indent=2)

        passed = len([e for e in self.events if e.get('kind') == 'check' and e.get('pass')])
        failed = len(self._failures)
        print(f"\n=== {self.name}: {passed} passed, {failed} failed ===")
        return failed == 0
