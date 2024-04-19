import os
import re
from xml.etree import ElementTree

# Convert themes from https://github.com/hundredrabbits/Themes

def hex_to_juce_color(hex_color):
    """Converts hex color format to JUCE color format."""
    return f'juce::Colour{{0xFF{hex_color.upper()[1:]}}}'

def get_element_color(element, element_id):
    """Retrieve color from an element by id."""
    elem = element.find(f".//*[@id='{element_id}']")
    if elem is not None:
        return elem.get('fill')
    else:
        print(f"Warning: '{element_id}' not found.")
        return '#000000'  # default color in case not found

def process_svg_file(file_path):
    """Extracts color data from the SVG file."""
    try:
        tree = ElementTree.parse(file_path)
        root = tree.getroot()

        # Handle different SVG namespaces
        namespaces = {'svg': 'http://www.w3.org/2000/svg'}
        ElementTree.register_namespace('', namespaces['svg'])

        # Get the background color from the rect element
        background_color = get_element_color(root, 'background')

        # Get the colors for foreground and background circles
        colors = {
            'fg_high': get_element_color(root, 'f_high'),
            'fg_med': get_element_color(root, 'f_med'),
            'fg_low': get_element_color(root, 'f_low'),
            'fg_inv': get_element_color(root, 'f_inv'),
            'bg_high': get_element_color(root, 'b_high'),
            'bg_med': get_element_color(root, 'b_med'),
            'bg_low': get_element_color(root, 'b_low'),
            'bg_inv': get_element_color(root, 'b_inv')
        }

        return {
            'background': hex_to_juce_color(background_color),
            'fg_high': hex_to_juce_color(colors['fg_high']),
            'fg_med': hex_to_juce_color(colors['fg_med']),
            'fg_low': hex_to_juce_color(colors['fg_low']),
            'fg_inv': hex_to_juce_color(colors['fg_inv']),
            'bg_high': hex_to_juce_color(colors['bg_high']),
            'bg_med': hex_to_juce_color(colors['bg_med']),
            'bg_low': hex_to_juce_color(colors['bg_low']),
            'bg_inv': hex_to_juce_color(colors['bg_inv'])
        }
    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return None

def main():
    output_file = "output.txt"
    with open(output_file, "w") as f_out:
        for filename in os.listdir('.'):
            if filename.endswith('.svg'):
                file_name_without_ext = os.path.splitext(filename)[0]
                colors = process_svg_file(filename)
                if colors:
                    f_out.write(f'{{"{file_name_without_ext}",\n    {{\n')
                    for key, value in colors.items():
                        f_out.write(f'        .{key} = {value},\n')
                    f_out.write('    }},\n\n')

if __name__ == "__main__":
    main()
