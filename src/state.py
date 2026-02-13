from typing import TypedDict, Optional, List, Annotated
from langchain_core.messages import BaseMessage
import operator


class AgentState(TypedDict, total=False):
    """State schema for the embedded code generation agent."""

    # Required inputs
    requirements: str

    # Run configuration
    task_name: str
    prompt_file: str  # Name of the prompt file used (e.g., "prompt_v2.txt")
    run_dir: str  # Path to current run directory (e.g., tasks/dht11/runs/2026-02-12_14-30-25)

    # Project metadata (set by manager)
    project_name: str
    active_platform: Optional[str]
    active_skills: List[str]
    active_skill_content: Optional[str]
    prepared_output_dir: Optional[str]
    prepared_code_path: Optional[str]

    # Artifacts from parallel nodes
    code_content: Optional[str]
    diagram_content: Optional[str]

    # Message history for multi-turn interactions
    messages: Annotated[List[BaseMessage], operator.add]

    # Debug logging - accumulates LLM call records
    debug_logs: Annotated[List[dict], operator.add]

    # Final output
    status_msg: str