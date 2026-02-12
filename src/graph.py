"""
LangGraph workflow definition for the embedded code generation agent.

Graph structure:
    manager -> [coder, diagram] -> assembler -> END

The manager node fans out to coder and diagram (parallel execution),
then both fan in to the assembler which writes the final output.
"""

from langgraph.graph import END, StateGraph

from src.nodes import assembler_node, coder_node, diagram_node, manager_node
from src.state import AgentState


def build_graph():
    """Build and compile the agent workflow graph."""
    workflow = StateGraph(AgentState)

    # Add nodes
    workflow.add_node("manager", manager_node)
    workflow.add_node("coder", coder_node)
    workflow.add_node("diagram", diagram_node)
    workflow.add_node("assembler", assembler_node)

    # Entry point
    workflow.set_entry_point("manager")

    # Parallel fan-out from manager
    workflow.add_edge("manager", "coder")
    workflow.add_edge("manager", "diagram")

    # Fan-in to assembler (waits for both coder and diagram)
    workflow.add_edge("coder", "assembler")
    workflow.add_edge("diagram", "assembler")

    # End
    workflow.add_edge("assembler", END)

    return workflow.compile()