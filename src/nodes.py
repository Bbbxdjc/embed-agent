"""
Node implementations for the embedded code generation workflow.

Nodes:
- manager_node: Plans the project and selects relevant skills
- prepare_workspace_node: Prepares output directories before code generation
- coder_node: Generates the main code file
- diagram_node: Placeholder for future diagram generation
- assembler_node: Writes output files to disk
"""

import json
import os
import re
import time
from datetime import datetime
from functools import lru_cache
from pathlib import Path
from typing import Any, List

from langchain_core.output_parsers import PydanticOutputParser
from langchain_core.prompts import ChatPromptTemplate
from langchain_openai import ChatOpenAI
from pydantic import BaseModel, Field

from src.loader import SkillRegistry
from src.state import AgentState

registry = SkillRegistry()
MODEL_NAME = "anthropic/claude-4.5-sonnet"
MODEL_TEMPERATURE = 0.0
MODEL_API_BASE = "https://openrouter.ai/api/v1"
MODEL_API_KEY_ENV = "OPENAI_API_KEY"


def configure_model(
    model_name: str,
    temperature: float = 0.0,
    api_base: str = "https://openrouter.ai/api/v1",
    api_key_env: str = "OPENAI_API_KEY",
) -> None:
    """
    Configure model settings used by all LLM nodes.

    Clearing get_model cache ensures runtime config takes effect immediately.
    """
    global MODEL_NAME, MODEL_TEMPERATURE, MODEL_API_BASE, MODEL_API_KEY_ENV  # pylint: disable=global-statement
    MODEL_NAME = model_name
    MODEL_TEMPERATURE = temperature
    MODEL_API_BASE = api_base
    MODEL_API_KEY_ENV = api_key_env
    get_model.cache_clear()


@lru_cache(maxsize=1)
def get_model() -> ChatOpenAI:
    """Return a cached LLM instance configured from runtime settings."""
    api_key = os.environ.get(MODEL_API_KEY_ENV) or os.environ.get("OPENROUTER_API_KEY")
    if not api_key:
        raise ValueError(
            f"Missing API key. Set {MODEL_API_KEY_ENV} (or OPENROUTER_API_KEY)."
        )

    return ChatOpenAI(
        model=MODEL_NAME,
        openai_api_key=api_key,
        openai_api_base=MODEL_API_BASE,
        temperature=MODEL_TEMPERATURE,
    )


def create_debug_log(
    node: str,
    input_messages: Any,
    output: Any,
    duration_ms: float,
    metadata: dict = None,
) -> dict:
    """Create a debug log entry for an LLM call."""
    return {
        "node": node,
        "timestamp": datetime.now().isoformat(),
        "duration_ms": round(duration_ms, 2),
        "input": input_messages,
        "output": output,
        "metadata": metadata or {},
    }


def extract_clean_code(raw_text: str) -> str:
    """
    Extract C/C++ code from an LLM response.

    Handles multiple formats:
    - ```c or ```cpp code blocks
    - Generic ``` code blocks
    - Raw code with conversational prefixes stripped
    """
    # Try ```c, ```cpp, or ```cc blocks
    match = re.search(r"```(?:c|cpp|cc)\n(.*?)```", raw_text, re.DOTALL)
    if match:
        return match.group(1).strip()

    # Try generic ``` blocks
    match = re.search(r"```\n(.*?)```", raw_text, re.DOTALL)
    if match:
        return match.group(1).strip()

    # Fallback: detect code by common starting patterns
    lines = raw_text.split("\n")
    code_lines = []
    started = False

    for line in lines:
        if not started:
            if line.strip().startswith(("#include", "//", "/*", "void", "int")):
                started = True
                code_lines.append(line)
        else:
            code_lines.append(line)

    if code_lines:
        return "\n".join(code_lines).strip()

    return raw_text.strip()


# --- Schema ---


class ProjectPlan(BaseModel):
    """Schema for the manager's project planning output."""

    project_name: str = Field(
        ..., description="Project name in snake_case (e.g., 'esp32_sensor_node')"
    )
    selected_skills: List[str] = Field(
        ..., description="List of relevant skill names (e.g., ['esp-idf', 'arduino'])"
    )


# --- Nodes ---


def manager_node(state: AgentState) -> dict:
    """
    Plan the project by analyzing requirements and selecting skills.

    Reads: requirements
    Writes: project_name, active_skills, active_skill_content, debug_logs
    """
    llm = get_model()
    available_skills = registry.scan_skills()

    parser = PydanticOutputParser(pydantic_object=ProjectPlan)

    prompt = ChatPromptTemplate.from_messages(
        [
            (
                "system",
                "You are a Project Planner. Analyze the request and output a JSON plan.\n"
                "1. Use snake_case for the project_name.\n"
                "2. Select relevant skills ONLY from the AVAILABLE SKILLS list.\n\n"
                "AVAILABLE SKILLS:\n{skills}\n\n"
                "{format_instructions}",
            ),
            ("user", "{request}"),
        ]
    )

    chain = prompt | llm | parser

    input_data = {
        "skills": available_skills,
        "format_instructions": parser.get_format_instructions(),
        "request": state["requirements"],
    }

    start_time = time.time()

    try:
        plan: ProjectPlan = chain.invoke(input_data)
        duration_ms = (time.time() - start_time) * 1000

        skill_content = registry.get_combined_skill_content(plan.selected_skills)

        debug_log = create_debug_log(
            node="manager",
            input_messages={
                "system": prompt.messages[0].prompt.template,
                "user": state["requirements"],
                "available_skills": available_skills,
            },
            output=plan.model_dump(),
            duration_ms=duration_ms,
            metadata={"parser": "PydanticOutputParser", "schema": "ProjectPlan"},
        )

        return {
            "project_name": plan.project_name,
            "active_skills": plan.selected_skills,
            "active_skill_content": skill_content,
            "debug_logs": [debug_log],
        }

    except Exception as e:
        duration_ms = (time.time() - start_time) * 1000

        debug_log = create_debug_log(
            node="manager",
            input_messages={"request": state["requirements"]},
            output={"error": str(e)},
            duration_ms=duration_ms,
            metadata={"status": "fallback"},
        )

        return {
            "project_name": "esp32_fallback",
            "active_skills": ["esp-idf"],
            "active_skill_content": "Use standard ESP-IDF best practices.",
            "debug_logs": [debug_log],
        }


def prepare_workspace_node(state: AgentState) -> dict:
    """
    Prepare output folders and target code file path before code generation.

    Reads: run_dir, project_name, active_skills
    Writes: prepared_output_dir, prepared_code_path, active_platform
    """
    project_name = state.get("project_name", "embedded_project")
    active_skills = state.get("active_skills", [])
    run_dir = state.get("run_dir", "./output")

    run_path = Path(run_dir)
    output_dir = run_path / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    if "arduino" in active_skills:
        code_path = output_dir / f"{project_name}.ino"
        active_platform = "arduino"
    else:
        main_dir = output_dir / "main"
        main_dir.mkdir(parents=True, exist_ok=True)
        code_path = main_dir / "main.c"
        active_platform = "esp-idf"

    return {
        "prepared_output_dir": str(output_dir),
        "prepared_code_path": str(code_path),
        "active_platform": active_platform,
    }


def coder_node(state: AgentState) -> dict:
    """
    Generate the main code file based on requirements and skill standards.

    Reads: requirements, project_name, active_skill_content
    Writes: code_content, messages, active_skills, debug_logs
    """
    llm = get_model()

    skill_instructions = state.get("active_skill_content") or "No specific standards."
    project_name = state.get("project_name", "embedded_project")
    active_skills = state.get("active_skills", [])

    system_prompt = f"""You are an expert Embedded Engineer. Generate ONLY the code for main.c or *.ino file.

Target Project: {project_name}

=== APPLICABLE STANDARDS ===
{skill_instructions}
============================

Task: Write the main C/C++ code file.

RULES:
1. Do NOT ask clarifying questions. Make reasonable engineering assumptions.
2. Output ONLY the code block (inside ```c wrapper).
3. Include all necessary headers based on the requirements.
4. Use reasonable GPIO pins if not specified."""

    messages = [("system", system_prompt), ("user", state["requirements"])]

    start_time = time.time()
    response = llm.invoke(messages)
    duration_ms = (time.time() - start_time) * 1000

    debug_log = create_debug_log(
        node="coder",
        input_messages={
            "system": system_prompt,
            "user": state["requirements"],
        },
        output=response.content,
        duration_ms=duration_ms,
        metadata={
            "project_name": project_name,
            "active_skills": active_skills,
        },
    )

    return {
        "code_content": response.content,
        "messages": [response],
        "active_skills": active_skills,
        "debug_logs": [debug_log],
    }


def diagram_node(state: AgentState) -> dict:  # noqa: ARG001  # pylint: disable=unused-argument
    """Placeholder for future diagram generation."""
    return {"diagram_content": ""}


def assembler_node(state: AgentState) -> dict:
    """
    Write the generated code and debug info to disk.

    For Arduino: Creates output/{project_name}.ino
    For ESP-IDF: Creates output/main/main.c with CMakeLists.txt

    Also writes:
    - debug.json: All LLM call logs
    - metadata.json: Run summary
    """
    project_name = state.get("project_name", "output_project")
    raw_code = state.get("code_content", "")
    active_skills = state.get("active_skills", [])
    active_platform = state.get("active_platform")
    run_dir = state.get("run_dir", "./output")
    task_name = state.get("task_name", "unknown")
    debug_logs = state.get("debug_logs", [])
    prepared_output_dir = state.get("prepared_output_dir")
    prepared_code_path = state.get("prepared_code_path")

    clean_code = extract_clean_code(raw_code)

    run_path = Path(run_dir)
    output_dir = Path(prepared_output_dir) if prepared_output_dir else run_path / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Write code files
    if prepared_code_path:
        code_path = Path(prepared_code_path)
    elif "arduino" in active_skills:
        code_path = output_dir / f"{project_name}.ino"
    else:
        main_dir = output_dir / "main"
        main_dir.mkdir(parents=True, exist_ok=True)
        code_path = main_dir / "main.c"

    code_path.parent.mkdir(parents=True, exist_ok=True)
    code_path.write_text(clean_code)

    if active_platform == "arduino" or code_path.suffix == ".ino":
        output_type = "arduino"
    else:
        main_dir = code_path.parent
        (output_dir / "CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.16)\n"
            f"include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)\n"
            f"project({project_name})"
        )
        (main_dir / "CMakeLists.txt").write_text(
            'idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")'
        )
        output_type = "esp-idf"

    # Write debug.json
    debug_path = run_path / "debug.json"
    debug_path.write_text(json.dumps(debug_logs, indent=2))

    # Write metadata.json
    metadata = {
        "task_name": task_name,
        "prompt_file": state.get("prompt_file", "unknown"),
        "project_name": project_name,
        "active_skills": active_skills,
        "output_type": output_type,
        "timestamp": datetime.now().isoformat(),
        "requirements": state.get("requirements", ""),
    }
    metadata_path = run_path / "metadata.json"
    metadata_path.write_text(json.dumps(metadata, indent=2))

    return {"status_msg": f"Project generated at {run_path}"}