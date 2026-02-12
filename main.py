"""
Entry point for the embedded code generation agent.

Usage:
    python main.py <task_directory>

Example:
    python main.py tasks/dht11_sensor

Task directory structure:
    tasks/dht11_sensor/
    ├── prompt.txt              # Required: The task requirements
    └── runs/                   # Auto-created: Run history
        └── 2026-02-12_14-30-25/
            ├── debug.json      # All LLM call logs
            ├── metadata.json   # Run summary
            └── output/         # Generated code
"""

import sys
from datetime import datetime
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()

from src.graph import build_graph


def main():
    if len(sys.argv) < 2:
        print("Usage: python main.py <task_directory>")
        print("Example: python main.py tasks/dht11_sensor")
        sys.exit(1)

    task_dir = Path(sys.argv[1])

    # Validate task directory
    if not task_dir.exists():
        print(f"Error: Task directory not found: {task_dir}")
        sys.exit(1)

    prompt_file = task_dir / "prompt.txt"
    if not prompt_file.exists():
        print(f"Error: prompt.txt not found in {task_dir}")
        sys.exit(1)

    # Load requirements
    requirements = prompt_file.read_text().strip()
    task_name = task_dir.name

    # Create timestamped run directory
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    run_dir = task_dir / "runs" / timestamp
    run_dir.mkdir(parents=True, exist_ok=True)

    print(f"Task: {task_name}")
    print(f"Run directory: {run_dir}")
    print(f"Requirements:\n{requirements}\n")
    print("Starting embedded code generation...\n")

    app = build_graph()

    inputs = {
        "requirements": requirements,
        "task_name": task_name,
        "run_dir": str(run_dir),
        "messages": [],
        "debug_logs": [],
    }

    for event in app.stream(inputs):
        for node_name, output in event.items():
            print(f"--- Node: {node_name} ---")

            if node_name == "manager":
                print(f"  Project: {output.get('project_name')}")
                print(f"  Skills: {output.get('active_skills')}")

            elif node_name == "assembler":
                print(f"  {output.get('status_msg')}")

    print(f"\nDebug logs saved to: {run_dir}/debug.json")
    print(f"Metadata saved to: {run_dir}/metadata.json")


if __name__ == "__main__":
    main()