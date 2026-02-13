"""
Entry point for the embedded code generation agent.

Usage:
    python main.py [task_directory] [prompt_file]

Examples:
    python main.py                                     # Fully driven by config.yaml
    python main.py tasks/dht11_sensor                  # Override task_dir from CLI
    python main.py tasks/dht11_sensor prompt.txt       # Override prompt file from CLI

Task directory structure:
    tasks/dht11_sensor/
    ├── prompt.txt              # Prompt option 1
    ├── prompt_v2.txt           # Prompt option 2
    ├── advanced.txt            # Prompt option 3
    └── runs/
        └── 2026-02-12_14-30-25/
            ├── debug.json
            ├── metadata.json
            └── output/
"""

import sys
from datetime import datetime
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()

from src.config import load_config
from src.graph import build_graph
from src.nodes import configure_model


def list_prompt_files(task_dir: Path) -> list[Path]:
    """Find all .txt files in the task directory (excluding runs/)."""
    return sorted([
        f for f in task_dir.glob("*.txt")
        if f.is_file()
    ])


def select_prompt_file(task_dir: Path, prompt_arg: str = None) -> Path:
    """
    Select a prompt file from the task directory.

    Args:
        task_dir: Path to task directory
        prompt_arg: Optional - filename or number to select

    Returns:
        Path to selected prompt file
    """
    prompt_files = list_prompt_files(task_dir)

    if not prompt_files:
        print(f"Error: No .txt files found in {task_dir}")
        sys.exit(1)

    # If only one file, use it automatically
    if len(prompt_files) == 1:
        return prompt_files[0]

    # If prompt_arg provided, try to match it
    if prompt_arg:
        # Try as filename
        for f in prompt_files:
            if f.name == prompt_arg:
                return f

        # Try as number (1-indexed)
        try:
            idx = int(prompt_arg) - 1
            if 0 <= idx < len(prompt_files):
                return prompt_files[idx]
        except ValueError:
            pass

        print(f"Error: '{prompt_arg}' not found. Available prompts:")
        for i, f in enumerate(prompt_files, 1):
            print(f"  {i}. {f.name}")
        sys.exit(1)

    # Interactive selection
    print(f"Multiple prompts found in {task_dir.name}/:")
    for i, f in enumerate(prompt_files, 1):
        # Show first line of each prompt as preview
        preview = f.read_text().split('\n')[0][:60]
        if len(preview) == 60:
            preview += "..."
        print(f"  {i}. {f.name}")
        print(f"     {preview}")

    print()
    choice = input("Select prompt (number or filename): ").strip()

    # Try as number
    try:
        idx = int(choice) - 1
        if 0 <= idx < len(prompt_files):
            return prompt_files[idx]
    except ValueError:
        pass

    # Try as filename
    for f in prompt_files:
        if f.name == choice:
            return f

    print(f"Error: Invalid selection '{choice}'")
    sys.exit(1)


def main():
    config = load_config("config.yaml")
    cli_task_dir = sys.argv[1] if len(sys.argv) > 1 else None
    cli_prompt_arg = sys.argv[2] if len(sys.argv) > 2 else None

    task_dir_str = cli_task_dir or config.input.task_dir
    if not task_dir_str:
        print("Error: task directory is required.")
        print("Provide it via CLI arg or config.yaml -> input.task_dir")
        sys.exit(1)

    task_dir = Path(task_dir_str)
    prompt_arg = cli_prompt_arg or config.input.prompt_file

    # Validate task directory
    if not task_dir.exists():
        print(f"Error: Task directory not found: {task_dir}")
        sys.exit(1)

    prompt_file = select_prompt_file(task_dir, prompt_arg)
    requirements = prompt_file.read_text().strip()
    prompt_file_name = prompt_file.name

    task_name = task_dir.name

    # Create timestamped run directory
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    run_dir = task_dir / "runs" / timestamp
    run_dir.mkdir(parents=True, exist_ok=True)

    configure_model(
        model_name=config.model.name,
        temperature=config.model.temperature,
        api_base=config.model.api_base,
        api_key_env=config.model.api_key_env,
    )

    print(f"Task: {task_name}")
    print(f"Prompt: {prompt_file_name}")
    print(f"Model: {config.model.name} (temperature={config.model.temperature})")
    print(f"Model.api_base: {config.model.api_base}")
    print(f"Model.api_key_env: {config.model.api_key_env}")
    print(f"Graph.enable_diagram: {config.graph.enable_diagram}")
    print(f"Run directory: {run_dir}")
    print(f"Requirements:\n{requirements}\n")
    print("Starting embedded code generation...\n")

    app = build_graph(enable_diagram=config.graph.enable_diagram)

    inputs = {
        "requirements": requirements,
        "task_name": task_name,
        "prompt_file": prompt_file_name,
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

            elif node_name == "persist":
                print(f"  {output.get('status_msg')}")

    print(f"\nDebug logs saved to: {run_dir}/debug.json")
    print(f"Metadata saved to: {run_dir}/metadata.json")


if __name__ == "__main__":
    main()