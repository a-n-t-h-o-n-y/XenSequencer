import matplotlib.pyplot as plt
import numpy as np

# Function to interpolate between HSL values
def interpolate_hsl(start, middle, end, num_points=8):
    def lerp(a, b, t):
        return a + t * (b - a)
    
    # Split the gradient into two sections: start → middle and middle → end
    mid_point = num_points // 2
    gradient = []
    
    for i in range(num_points):
        t = i / (num_points - 1)
        if i < mid_point:
            section_t = i / (mid_point - 1) if mid_point > 1 else 0
            gradient.append((
                lerp(start[0], middle[0], section_t),
                lerp(start[1], middle[1], section_t),
                lerp(start[2], middle[2], section_t),
            ))
        else:
            section_t = (i - mid_point) / (num_points - mid_point - 1) if num_points - mid_point - 1 > 0 else 0
            gradient.append((
                lerp(middle[0], end[0], section_t),
                lerp(middle[1], end[1], section_t),
                lerp(middle[2], end[2], section_t),
            ))
    return gradient

# Convert HSL to RGB
def hsl_to_rgb(h, s, l):
    c = (1 - abs(2 * l - 1)) * s
    x = c * (1 - abs((h / 60) % 2 - 1))
    m = l - c / 2

    if 0 <= h < 60:
        r, g, b = c, x, 0
    elif 60 <= h < 120:
        r, g, b = x, c, 0
    elif 120 <= h < 180:
        r, g, b = 0, c, x
    elif 180 <= h < 240:
        r, g, b = 0, x, c
    elif 240 <= h < 300:
        r, g, b = x, 0, c
    else:
        r, g, b = c, 0, x

    return (r + m, g + m, b + m)

# Plot the gradient squares
def plot_gradient(gradient_hsl):
    gradient_rgb = [hsl_to_rgb(h, s, l) for h, s, l in gradient_hsl]
    fig, ax = plt.subplots(figsize=(2, 6))
    
    for i, color in enumerate(gradient_rgb):
        ax.add_patch(plt.Rectangle((0, i), 1, 1, color=color))
    
    ax.set_xlim(0, 1)
    ax.set_ylim(0, len(gradient_rgb))
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_aspect('equal')
    plt.gca().invert_yaxis()  # Higher on top
    plt.title("Gradient Squares (Higher on Top)")
    plt.show()

# Input: Beginning, middle, and end HSL values
start_hsl = (0, 0.9, 0.4)  # Red, vibrant, dark
middle_hsl = (180, 0.8, 0.5)  # Cyan, vibrant, medium
end_hsl = (270, 0.7, 0.8)  # Violet, less vibrant, bright

# Generate and display gradient
gradient_hsl = interpolate_hsl(end_hsl, middle_hsl, start_hsl)
plot_gradient(gradient_hsl)
