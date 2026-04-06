"""
Test: pressing S opens the Command Palette ("Design Shortcuts").
Verifies the Fusion 360 alignment change.
"""
import sys
sys.path.insert(0, '/home/andrii/Projects/kernelCAD-desktop/scripts/gui_test')
from lib_kctest import KCTest

t = KCTest('s_key_palette')

# 1. Window must exist
t.expect_window_exists('kernelCAD')

# 2. Take baseline screenshot
geom = t.get_window_geometry()
print(f"Window geometry: {geom}")

# Click in the viewport center to make sure focus is there
if geom:
    x = geom[0] + geom[2] // 2
    y = geom[1] + geom[3] // 2
else:
    x, y = 960, 540
t.click(x, y, label='focus_viewport')
t.wait(0.4)

before = t.screenshot('before_s')

# 3. Press S
t.key('s', label='press_s')
t.wait(0.6)  # Let palette appear

after = t.screenshot('after_s')

# 4. Verify pixels changed (palette should have opened)
t.expect_pixel_changed(before, after, min_pct=0.5)

# 5. Type "extrude" to verify the search works
t.typetext('extrude', label='search_extrude')
t.wait(0.5)
after_search = t.screenshot('after_search')
t.expect_pixel_changed(after, after_search, min_pct=0.1)

# 6. Press Escape to dismiss
t.key('Escape', label='dismiss')
t.wait(0.4)
after_escape = t.screenshot('after_escape')

# Verify palette dismissed (back to baseline-ish)
t.expect_pixel_changed(after_search, after_escape, min_pct=0.5)

ok = t.finish()
sys.exit(0 if ok else 1)
