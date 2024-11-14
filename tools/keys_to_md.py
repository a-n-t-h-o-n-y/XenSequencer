import yaml
import re
import sys
import os
from pathlib import Path

def get_input_mode_name(short_mode):
    mode_translations = {
        'p': 'Pitch',
        'v': 'Velocity',
        'd': 'Delay',
        'g': 'Gate',
        'c': 'Scale',
        'm': 'Scale Mode'
    }
    return mode_translations.get(short_mode, short_mode)

def extract_input_mode(key):
    match = re.match(r'\[(.*?)\]', key)
    if match:
        short_mode = match.group(1)
        return get_input_mode_name(short_mode)
    return "---"

def create_markdown_tables(yaml_file):
    try:
        with open(yaml_file, 'r') as f:
            data = yaml.safe_load(f)

        markdown = f"# Keybindings (v{data.pop('version')})\n\n"

        for section, bindings in data.items():
            markdown += f"## {section}\n\n"
            markdown += "| Input Mode | Key | Command |\n"
            markdown += "|------------|-----|--------|\n"

            for key, command in bindings.items():
                if isinstance(key, str) and not key.startswith('#'):
                    input_mode = extract_input_mode(key)
                    clean_key = re.sub(r'\[(.*?)\]\s*\+\s*', '', key)
                    clean_command = command.strip('"\'')
                    markdown += f"| {input_mode} | `{clean_key}` | `{clean_command}` |\n"

            markdown += "\n"

        return markdown
    except FileNotFoundError:
        print(f"Error: File '{yaml_file}' not found")
        sys.exit(1)
    except yaml.YAMLError as e:
        print(f"Error parsing YAML file: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python keys_to_md.py <path_to_keys.yml>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = Path(input_file).with_suffix('.md')

    markdown_content = create_markdown_tables(input_file)
    with open(output_file, 'w') as f:
        f.write(markdown_content)

    print(f"Documentation generated: {output_file}")