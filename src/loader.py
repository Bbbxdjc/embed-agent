from pathlib import Path
from typing import Dict, Optional

import yaml


class SkillRegistry:
    """
    Registry for discovering and loading skills from SKILL.md files.

    Skills follow a format inspired by Claude Code's skill system:
    - Each skill lives in its own directory under skills_dir
    - Contains a SKILL.md file with YAML frontmatter (name, description)
    - The markdown body contains instructions/standards for the LLM
    """

    def __init__(self, skills_dir: str = "./skills"):
        self.skills_dir = Path(skills_dir)
        self.descriptions: Dict[str, str] = {}
        self._cache: Dict[str, str] = {}

    def scan_skills(self) -> str:
        """
        Scan for available skills and return a formatted list.

        Returns:
            Formatted string of skills for LLM context:
            "- skill_name: description\\n- skill_name2: description2"
        """
        self.descriptions.clear()

        for skill_path in self.skills_dir.rglob("SKILL.md"):
            self._parse_skill_metadata(skill_path)

        return "\n".join(f"- {name}: {desc}" for name, desc in self.descriptions.items())

    def _parse_skill_metadata(self, path: Path) -> None:
        """Parse YAML frontmatter from a SKILL.md file."""
        try:
            content = path.read_text()
            parts = content.split("---", 2)

            if len(parts) >= 3:
                meta = yaml.safe_load(parts[1])
                name = meta.get("name", path.parent.name)
                desc = meta.get("description", "No description")
                self.descriptions[name] = desc

        except Exception as e:
            print(f"Warning: Failed to parse skill at {path}: {e}")

    def load_skill_content(self, skill_name: str) -> Optional[str]:
        """
        Load the markdown body content for a specific skill.

        Args:
            skill_name: Name of the skill to load

        Returns:
            The skill's instruction content (without frontmatter), or None if not found
        """
        if skill_name in self._cache:
            return self._cache[skill_name]

        skill_path = self.skills_dir / skill_name / "SKILL.md"

        if not skill_path.exists():
            return None

        content = skill_path.read_text()
        parts = content.split("---", 2)

        # Return only the body (after frontmatter)
        body = parts[-1].strip() if len(parts) >= 3 else content.strip()
        self._cache[skill_name] = body

        return body

    def get_combined_skill_content(self, skill_names: list[str]) -> str:
        """
        Load and combine content from multiple skills.

        Args:
            skill_names: List of skill names to load

        Returns:
            Combined skill content with headers
        """
        sections = []

        for name in skill_names:
            content = self.load_skill_content(name)
            if content:
                sections.append(f"=== SKILL: {name} ===\n{content}")

        return "\n\n".join(sections)