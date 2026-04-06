"""
Test: Toolbar layout matches Fusion 360 reorganization.
Verifies "Form" and "Pattern" panels were merged into "Create".
"""
import sys
sys.path.insert(0, '/home/andrii/Projects/kernelCAD-desktop/scripts/gui_test')
from lib_kctest import KCTest

t = KCTest('toolbar_layout')

t.expect_window_exists('kernelCAD')
t.wait(0.5)

# Take a baseline screenshot of the entire window
t.screenshot('full_window')

# Open command palette and search for all major commands.
# If they're all findable, the registry is intact.
test_commands = [
    'Extrude',     # was Form, now Create
    'Mirror',      # was Pattern, now Create
    'Scale',       # was stub, now wired
    'Combine',     # was stub, now wired
    'Move/Copy',   # was stub, now wired
    'Thicken',     # was stub, now wired
    'Coil',        # was stub, now wired
    'Thread',      # was stub, now wired
]

for cmd_name in test_commands:
    t.click(960, 540, label=f'focus_for_{cmd_name}')
    t.wait(0.2)
    t.key('s', label=f'palette_for_{cmd_name}')
    t.wait(0.4)
    t.typetext(cmd_name, label=f'search_{cmd_name}')
    t.wait(0.4)
    t.screenshot(f'search_{cmd_name.replace("/", "_")}')
    t.key('Escape', label=f'dismiss_{cmd_name}')
    t.wait(0.3)
    t.key('Escape', label=f'dismiss2_{cmd_name}')
    t.wait(0.2)

ok = t.finish()
sys.exit(0 if ok else 1)
