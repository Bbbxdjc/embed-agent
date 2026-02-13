"""
LangGraph workflow definition for the embedded code generation agent.

Graph structure:
    manager -> prepare_workspace -> coder -> assembler -> END

Diagram support is kept behind a switch so it can be re-enabled without
changing node implementations.
"""

from langgraph.graph import END, StateGraph

from src.nodes import (
    assembler_node,
    coder_node,
    diagram_node,
    manager_node,
    prepare_workspace_node,
)
from src.state import AgentState


def build_graph(enable_diagram: bool = False):
    """Build and compile the agent workflow graph."""
    workflow = StateGraph(AgentState)

    # Add nodes
    workflow.add_node("manager", manager_node)
    workflow.add_node("prepare_workspace", prepare_workspace_node)
    workflow.add_node("coder", coder_node)
    workflow.add_node("diagram", diagram_node)
    workflow.add_node("assembler", assembler_node)

    # Entry point
    workflow.set_entry_point("manager")

    # Main path: manager -> prepare_workspace -> coder
    workflow.add_edge("manager", "prepare_workspace")
    workflow.add_edge("prepare_workspace", "coder")

    if enable_diagram:
        # Optional branch: manager -> diagram, then fan-in at assembler.
        workflow.add_edge("manager", "diagram")
        workflow.add_edge("diagram", "assembler")

    workflow.add_edge("coder", "assembler")

    # End
    workflow.add_edge("assembler", END)

    return workflow.compile()