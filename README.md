# CVis: C Visualizer of Parallel Coordinates & Scatter Plot
![Parallel Coordinates with Scatter Plot](pc_scatterplot.png)
Image of Fisher Iris in Parallel Coordinates with Scatter Plot.

### About

Renders CSV in Parallel Coordinates + Scatter Plot, handles large datasets. Zoom, pan, stretch, scale using keyboard, invert of attributes with mouse click

Supports multi-monitor for expanding data to better locate patterns.

Star below an axis denote inversion of the axis where range [0, 1] -> [1, 0].

Written in C using OpenGL and FreeGLUT.

### Controls

Controls for this program are:

| Control     | Action      |
| ----------- | ----------- |
| wasd        | pan         |
| qe          | scale x     |
| rf          | scale y     |
| left click  | invert axis |
