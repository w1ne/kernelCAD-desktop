# kernelcad-plugin
# name: Git Sync
# description: Version control your CAD designs with Git. Auto-commits on save, push/pull to GitHub.
# author: kernelCAD
# version: 1.0

"""
Git Sync Plugin for kernelCAD

Usage:
  1. Open a design and save it (Ctrl+S) to a folder
  2. Run Plugins → Git Sync → it will:
     - Initialize a git repo if needed
     - Commit the current .kcd file with a descriptive message
     - Push to remote if configured

Setup remote:
  cd /path/to/your/design/folder
  git remote add origin git@github.com:user/my-designs.git

The plugin auto-generates:
  - Commit messages from the feature timeline ("Add Extrude 25mm, Fillet 3mm")
  - .gitignore for STL files and build artifacts
  - README.md with design stats
"""

import subprocess
import os
import json
import sys
from pathlib import Path
from datetime import datetime

# Plugin parameters shown in dialog
parameters = [
    {"name": "action", "type": "enum", "default": "commit",
     "options": ["commit", "push", "pull", "status", "log", "init"],
     "label": "Action"},
    {"name": "message", "type": "string", "default": "",
     "label": "Commit Message (auto if empty)"},
    {"name": "auto_export_step", "type": "bool", "default": True,
     "label": "Auto-export STEP on commit"},
]


def git(*args, cwd=None):
    """Run a git command and return stdout."""
    result = subprocess.run(
        ["git"] + list(args),
        cwd=cwd, capture_output=True, text=True, timeout=30
    )
    return result.returncode, result.stdout.strip(), result.stderr.strip()


def find_project_dir():
    """Find the directory containing the .kcd file."""
    # Look for the most recent .kcd file in common locations
    home = Path.home()
    candidates = [
        Path.cwd(),
        home / "Documents",
        home / "Projects",
        home,
    ]

    for d in candidates:
        if not d.exists():
            continue
        for kcd in d.glob("*.kcd"):
            return str(kcd.parent), kcd.name

    return str(Path.cwd()), None


def init_repo(project_dir):
    """Initialize a git repo with proper .gitignore."""
    code, _, _ = git("init", cwd=project_dir)
    if code != 0:
        return False, "Failed to initialize git repo"

    # Create .gitignore
    gitignore = os.path.join(project_dir, ".gitignore")
    if not os.path.exists(gitignore):
        with open(gitignore, "w") as f:
            f.write("""# kernelCAD project
*.stl
*.obj
build/
__pycache__/
*.pyc
.DS_Store
autosave/
*.autosave.kcd
""")

    # Initial commit
    git("add", "-A", cwd=project_dir)
    git("commit", "-m", "Initial kernelCAD project", cwd=project_dir)

    return True, "Git repo initialized"


def generate_commit_message(project_dir, kcd_file):
    """Generate a descriptive commit message from the .kcd file."""
    if not kcd_file:
        return "Update design"

    kcd_path = os.path.join(project_dir, kcd_file)
    if not os.path.exists(kcd_path):
        return "Update design"

    try:
        with open(kcd_path, "r") as f:
            data = json.load(f)

        features = data.get("features", [])
        if not features:
            return f"Update {kcd_file}"

        # Summarize the last few features
        recent = features[-3:]  # last 3 features
        parts = []
        for feat in recent:
            name = feat.get("name", feat.get("type", "Feature"))
            parts.append(name)

        total = len(features)
        summary = ", ".join(parts)
        timestamp = datetime.now().strftime("%H:%M")

        return f"[{timestamp}] {kcd_file}: {summary} ({total} features total)"

    except (json.JSONDecodeError, KeyError):
        return f"Update {kcd_file}"


def generate_readme(project_dir, kcd_file):
    """Generate a README.md with design stats."""
    if not kcd_file:
        return

    kcd_path = os.path.join(project_dir, kcd_file)
    if not os.path.exists(kcd_path):
        return

    try:
        with open(kcd_path, "r") as f:
            data = json.load(f)

        features = data.get("features", [])
        feature_types = {}
        for f_data in features:
            ft = f_data.get("type", "Unknown")
            feature_types[ft] = feature_types.get(ft, 0) + 1

        readme_path = os.path.join(project_dir, "README.md")
        with open(readme_path, "w") as f:
            f.write(f"# {Path(kcd_file).stem}\n\n")
            f.write(f"CAD design created with [kernelCAD](https://github.com/w1ne/kernelCAD-desktop)\n\n")
            f.write(f"## Design Stats\n\n")
            f.write(f"- **Features:** {len(features)}\n")
            for ft, count in sorted(feature_types.items()):
                f.write(f"  - {ft}: {count}\n")
            f.write(f"- **Last modified:** {datetime.now().strftime('%Y-%m-%d %H:%M')}\n")
            f.write(f"\n## Files\n\n")
            f.write(f"- `{kcd_file}` — Parametric design (open with kernelCAD)\n")

            # Check for exports
            exports_dir = os.path.join(project_dir, "exports")
            if os.path.exists(exports_dir):
                for exp in os.listdir(exports_dir):
                    f.write(f"- `exports/{exp}`\n")

    except (json.JSONDecodeError, KeyError):
        pass


def do_commit(project_dir, kcd_file, message, auto_export_step):
    """Commit current state."""
    # Check if repo exists
    code, _, _ = git("rev-parse", "--git-dir", cwd=project_dir)
    if code != 0:
        ok, msg = init_repo(project_dir)
        if not ok:
            return msg

    # Auto-export STEP if enabled
    if auto_export_step and kcd_file:
        exports_dir = os.path.join(project_dir, "exports")
        os.makedirs(exports_dir, exist_ok=True)
        step_path = os.path.join(exports_dir, Path(kcd_file).stem + ".step")

        # Use kernelcad-cli to export
        try:
            cli = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "..", "build", "src", "kernelcad-cli")
            if os.path.exists(cli):
                kcd_path = os.path.join(project_dir, kcd_file)
                cmds = (
                    f'{{"cmd":"load","id":1,"path":"{kcd_path}"}}\n'
                    f'{{"cmd":"exportStep","id":2,"path":"{step_path}"}}\n'
                )
                subprocess.run([cli], input=cmds, capture_output=True, text=True, timeout=30)
        except Exception:
            pass  # export is best-effort

    # Generate README
    generate_readme(project_dir, kcd_file)

    # Auto-generate commit message if not provided
    if not message:
        message = generate_commit_message(project_dir, kcd_file)

    # Stage and commit
    git("add", "-A", cwd=project_dir)

    # Check if there are changes to commit
    code, status, _ = git("status", "--porcelain", cwd=project_dir)
    if not status:
        return "Nothing to commit — no changes detected"

    code, out, err = git("commit", "-m", message, cwd=project_dir)
    if code != 0:
        return f"Commit failed: {err}"

    return f"Committed: {message}"


def do_push(project_dir):
    """Push to remote."""
    code, out, err = git("push", cwd=project_dir)
    if code != 0:
        if "no upstream" in err.lower() or "no remote" in err.lower():
            return "No remote configured. Run: git remote add origin <url>"
        return f"Push failed: {err}"
    return f"Pushed successfully"


def do_pull(project_dir):
    """Pull from remote."""
    code, out, err = git("pull", cwd=project_dir)
    if code != 0:
        return f"Pull failed: {err}"
    return f"Pulled: {out}"


def do_status(project_dir):
    """Show git status."""
    code, out, _ = git("status", "--short", cwd=project_dir)
    if code != 0:
        return "Not a git repo. Run with action='init' first."
    if not out:
        return "Clean — no uncommitted changes"
    return f"Changed files:\n{out}"


def do_log(project_dir):
    """Show recent git log."""
    code, out, _ = git("log", "--oneline", "-10", cwd=project_dir)
    if code != 0:
        return "No git history"
    return f"Recent commits:\n{out}"


def run(params):
    """Main entry point."""
    action = params.get("action", "commit")
    message = params.get("message", "")
    auto_export = params.get("auto_export_step", True)

    project_dir, kcd_file = find_project_dir()

    if action == "init":
        ok, msg = init_repo(project_dir)
        print(json.dumps({"ok": ok, "message": msg}))
    elif action == "commit":
        result = do_commit(project_dir, kcd_file, message, auto_export)
        print(json.dumps({"ok": True, "message": result}))
    elif action == "push":
        result = do_push(project_dir)
        print(json.dumps({"ok": True, "message": result}))
    elif action == "pull":
        result = do_pull(project_dir)
        print(json.dumps({"ok": True, "message": result}))
    elif action == "status":
        result = do_status(project_dir)
        print(json.dumps({"ok": True, "message": result}))
    elif action == "log":
        result = do_log(project_dir)
        print(json.dumps({"ok": True, "message": result}))
    else:
        print(json.dumps({"ok": False, "message": f"Unknown action: {action}"}))
