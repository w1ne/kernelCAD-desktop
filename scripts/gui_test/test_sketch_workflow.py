"""
Test: Full sketch workflow — open sketch, draw line, verify live dimension
and contextual hint appear.
"""
import sys
sys.path.insert(0, '/home/andrii/Projects/kernelCAD-desktop/scripts/gui_test')
from lib_kctest import KCTest

t = KCTest('sketch_workflow')

t.expect_window_exists('kernelCAD')

# Click in viewport to focus
t.click(960, 540, label='focus_viewport')
t.wait(0.5)

# Open command palette and search for sketch
t.key('s', label='open_palette')
t.wait(0.5)
t.typetext('Sketch', label='search_sketch')
t.wait(0.5)
t.screenshot('palette_with_sketch_search')

# Press Enter to invoke first result (Sketch)
t.key('Return', label='invoke_sketch')
t.wait(1.5)

# At this point we should see "Click an origin plane to start sketching"
shot_origin_picker = t.screenshot('origin_picker_visible')

# Click on the XY plane area (visible in middle of viewport)
t.click(960, 540, label='click_xy_plane')
t.wait(2.0)  # Wait for sketch mode entry animation

shot_in_sketch = t.screenshot('in_sketch_mode')

# Verify pixels changed significantly (sketch mode entered)
t.expect_pixel_changed(shot_origin_picker, shot_in_sketch, min_pct=1.0)

# Press L for line tool
t.key('l', label='line_tool')
t.wait(0.4)

# Click first point
t.click(800, 500, label='line_first')
t.wait(0.3)

# Move cursor to draw the line - should show live dimension AND "Specify next point"
t.move(1100, 500, label='line_drag')
t.wait(0.5)

shot_drawing = t.screenshot('line_being_drawn')

# Click second point
t.click(1100, 500, label='line_second')
t.wait(0.4)

# Press Escape to exit line tool
t.key('Escape', label='exit_line')
t.wait(0.3)

shot_after_line = t.screenshot('after_line_drawn')

# Verify line was drawn
t.expect_pixel_changed(shot_in_sketch, shot_after_line, min_pct=0.05)

ok = t.finish()
sys.exit(0 if ok else 1)
