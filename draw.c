#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>

#include <GL/freeglut.h>

#define MAX_LINE_LENGTH 1024

#define DEBUG FALSE

typedef struct {
    char* class_name;
    float r, g, b;
} ClassInfo;

float** global_data = NULL;
int global_rows = 0, global_cols = 0, global_class_col_index = 0;
float translate_x = 0.0f, translate_y = 0.0f, stretch_factor_x = 1.0f, stretch_factor_y = 1.0f;
float scale = 1.0f;
bool* axis_inverted = NULL;

ClassInfo* class_info = NULL;
int num_classes = 0;
int hovered_row = -1;

float point_to_line_dist(float px, float py, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float mag = sqrtf(dx * dx + dy * dy);
    dx /= mag;
    dy /= mag;
    float lambda = dx * (px - x1) + dy * (py - y1);
    lambda = fmax(0, fmin(mag, lambda));
    float closest_x = x1 + lambda * dx;
    float closest_y = y1 + lambda * dy;
    return sqrtf((px - closest_x) * (px - closest_x) + (py - closest_y) * (py - closest_y));
}

void draw_star(float cx, float cy, float size) {
    glColor3f(0.0f, 0.0f, 0.0f); // Set color to black for the star
    glBegin(GL_LINES);
    // Draw the star as a series of lines
    for (int i = 0; i < 5; ++i) {
        float angle = M_PI / 5 * i * 2; // angle in radians
        float x1 = cx + size * cos(angle);
        float y1 = cy + size * sin(angle);
        float x2 = cx + size * cos(angle + M_PI);
        float y2 = cy + size * sin(angle + M_PI);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
}

void draw_axes(int cols) {
    glColor3f(0.0f, 0.0f, 0.0f); // Set color for axes (black)

    for (int col = 0; col < cols; col++) {
        if (col == global_class_col_index) continue; // Skip the 'class' column

        float x = (col / (float)(cols - 1)) * stretch_factor_x; // Apply stretch factor

        glBegin(GL_LINES);
            glVertex2f(x, 0.0f); // Start of the line (bottom)
            glVertex2f(x, stretch_factor_y); // End of the line (top)
        glEnd();
    }
}

void hsv_to_rgb(float h, float s, float v, float* r, float* g, float* b) {
    int i;
    float f, p, q, t;

    h = fmodf(h, 360.0f);
    s = s > 1.0f ? 1.0f : s;
    v = v > 1.0f ? 1.0f : v;

    if (s == 0) {
        // Achromatic (grey)
        *r = *g = *b = v;
        return;
    }

    h /= 60; // sector 0 to 5
    i = floor(h);
    f = h - i; // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));
    switch (i) {
        case 0:
            *r = v;
            *g = t;
            *b = p;
            break;
        case 1:
            *r = q;
            *g = v;
            *b = p;
            break;
        case 2:
            *r = p;
            *g = v;
            *b = t;
            break;
        case 3:
            *r = p;
            *g = q;
            *b = v;
            break;
        case 4:
            *r = t;
            *g = p;
            *b = v;
            break;
        default: // case 5:
            *r = v;
            *g = p;
            *b = q;
            break;
    }
}

// Helper function to trim whitespace from a string
char* trim(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

// Function to get the index of a class label
int get_class_index(ClassInfo** class_info, int* num_classes, const char* class_label) {
    // Check if class label already exists
    for (int i = 0; i < *num_classes; i++) {
        if (strcmp((*class_info)[i].class_name, class_label) == 0) {
            return i; // Class label found, return index
        }
    }

    // Class label not found, add it to class_info
    ClassInfo* new_class_info = realloc(*class_info, (*num_classes + 1) * sizeof(ClassInfo));
    if (new_class_info == NULL) {
        perror("Memory allocation failed for new_class_info");
        return -1;
    }
    *class_info = new_class_info;
    (*class_info)[*num_classes].class_name = strdup(class_label);

    // Assign a placeholder color, actual color assignment can be done in assign_colors function
    (*class_info)[*num_classes].r = 0.0f;
    (*class_info)[*num_classes].g = 0.0f;
    (*class_info)[*num_classes].b = 0.0f;
    
    if (DEBUG) {
        printf("Class Label: '%s', Assigned Index: %d\n", class_label, *num_classes);
    }
    // Increase the class count and return the new class index
    return (*num_classes)++;
}

// Function to assign colors to each class
void assign_colors(ClassInfo* class_info, int num_classes) {
    float hue_step = 360.0f / num_classes;
    for (int i = 0; i < num_classes; i++) {
        float h = i * hue_step;
        hsv_to_rgb(h, 1.0f, 1.0f, &(class_info[i].r), &(class_info[i].g), &(class_info[i].b));
    }
}

// Extracts a column value from a CSV line
char* get_column_value(char* line, int col_index) {
    const char *start, *end;
    int col = 0;

    start = line;
    while (col < col_index) {
        start = strchr(start, ',');
        if (start == NULL) return NULL; // Column index out of range
        start++; // Move past the comma
        col++;
    }

    end = strchr(start, ',');
    if (end == NULL) {
        end = line + strlen(line); // Last column
    }

    size_t len = end - start;
    char *value = (char *)malloc(len + 1);
    if (value == NULL) {
        perror("Memory allocation failed for column value");
        return NULL;
    }

    strncpy(value, start, len);
    value[len] = '\0';
    printf(trim(value));
    return trim(value);
}

void keyboard(unsigned char key, int x, int y) {
    const float translate_increment = 0.01f;
    const float scale_increment = 0.01f;
    const float stretch_increment = 0.01f;

    switch (key) {
        case 27: // exit
            exit(0);
            break;
        case 'r': // increase y stretch
            stretch_factor_y += stretch_increment;
            break;
        case 'f': // decrease y stretch
            stretch_factor_y = (stretch_factor_y > stretch_increment) ? stretch_factor_y - stretch_increment : 0.1f;
            break;
        case 's': // pan down
            translate_y += translate_increment;
            break;
        case 'w': // pan up
            translate_y -= translate_increment;
            break;
        case 'd': // pan right
            translate_x -= translate_increment;
            break;
        case 'a': // pan left
            translate_x += translate_increment;
            break;
        case 'q': // shrink
            stretch_factor_x = (stretch_factor_x > stretch_increment) ? stretch_factor_x - stretch_increment : 0.1f;
            break;
        case 'e': // stretch
            stretch_factor_x += stretch_increment;
            break;
        case '-': // zoom in
            scale += scale_increment;
            break;
        case '+': // zoom out
            scale = (scale > scale_increment) ? scale - scale_increment : scale_increment;
            break;
        default:
            if (DEBUG) {
                printf("%d\n", key);
            }
    }
    if (DEBUG) {
        printf("Transforms %d, %d, %d, %d, %d\n", stretch_factor_x, stretch_factor_y, scale, translate_x, translate_y);
    }
    glutPostRedisplay();
}

void normalize_data(float** data, int rows, int cols, float* min_vals, float* max_vals) {
    for (int col = 0; col < cols; col++) {
        if (col == global_class_col_index) continue; // Skip normalization for the class column

        // Find min and max values for each column
        min_vals[col] = data[0][col];
        max_vals[col] = data[0][col];
        for (int row = 1; row < rows; row++) {
            if (data[row][col] < min_vals[col]) min_vals[col] = data[row][col];
            if (data[row][col] > max_vals[col]) max_vals[col] = data[row][col];
        }

        // Normalize data
        for (int row = 0; row < rows; row++) {
            data[row][col] = (data[row][col] - min_vals[col]) / (max_vals[col] - min_vals[col]);
        }
    }
}

int count_columns(const char* line) {
    int cols = 0;
    const char* tmp = line;
    while (*tmp) {
        if (*tmp == ',') cols++;
        tmp++;
    }
    return cols + 1; // Add 1 for the last column
}

float** load_csv(const char* filename, int* rows, int* cols, int* class_col_index, ClassInfo** class_info, int* num_classes) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    char line[MAX_LINE_LENGTH];

    // Read the header line and count columns
    if (fgets(line, MAX_LINE_LENGTH, file) == NULL) {
        fclose(file);
        return NULL;
    }
    *cols = count_columns(line);

    // Find the 'class' column index
    *class_col_index = -1;
    char* token = strtok(line, ",");
    for (int i = 0; token != NULL; i++) {
        char* trimmed_token = trim(token);
        if (strcasecmp(trimmed_token, "class") == 0) {
            *class_col_index = i;
            break;
        }
        token = strtok(NULL, ",");
    }

    if (*class_col_index == -1) {
        printf("Error: 'class' column not found\n");
        fclose(file);
        return NULL;
    }

    // Count rows
    *rows = 0;
    while (fgets(line, MAX_LINE_LENGTH, file) != NULL) {
        (*rows)++;
    }

    // Allocate memory for data
    float** data = (float**)malloc(*rows * sizeof(float*));
    for (int i = 0; i < *rows; i++) {
        data[i] = (float*)malloc(*cols * sizeof(float));
    }

    // Rewind file to read data
    rewind(file);
    fgets(line, MAX_LINE_LENGTH, file); // Skip header line

    // Process data and class labels
    *num_classes = 0;
    *class_info = (ClassInfo*)malloc(sizeof(ClassInfo));
    for (int i = 0; fgets(line, MAX_LINE_LENGTH, file) != NULL; i++) {
        token = strtok(line, ",");
        for (int j = 0; j < *cols; j++) {
            if (j == *class_col_index) {
                char* class_label = trim(strdup(token));
                int label_index = get_class_index(class_info, num_classes, class_label);
                free(class_label);
                data[i][j] = (float)label_index;

                // Debug print to check the class index stored in global_data
                if (DEBUG) {
                    printf("Row %d, Class Label: '%s', Stored Index: %d\n", i, class_label, label_index);
                }
            } else {
                data[i][j] = atof(token); // Convert to float and store in data array
            }
            token = strtok(NULL, ",");
        }
    }

    fclose(file);

    // Assign colors to each unique class
    assign_colors(*class_info, *num_classes);

    return data;
}

int find_or_add_class_label(char*** unique_labels, int* num_labels, const char* label) {
    // Check if the label already exists
    for (int i = 0; i < *num_labels; i++) {
        if (strcmp((*unique_labels)[i], label) == 0) {
            return i; // Label found, return index
        }
    }

    // Label not found, add it
    char** new_labels = realloc(*unique_labels, (*num_labels + 1) * sizeof(char*));
    if (!new_labels) {
        perror("Memory allocation failed for new_labels");
        return -1;
    }

    new_labels[*num_labels] = strdup(label);
    if (!new_labels[*num_labels]) {
        perror("Memory allocation failed for label copy");
        return -1;
    }

    *unique_labels = new_labels;
    return (*num_labels)++;
}

void draw_parallel_coordinates(float** data, int rows, int cols, int class_col_index, ClassInfo* class_info, int num_classes) {
    // Draw all non-highlighted polylines first
    for (int row = 0; row < rows; row++) {
        if (row == hovered_row) continue; // Skip the hovered row for now
        
        int class_index = (int)data[row][class_col_index];
        glColor3f(class_info[class_index].r, class_info[class_index].g, class_info[class_index].b);
        glBegin(GL_LINE_STRIP);
        for (int col = 0; col < cols; col++) {
            if (col == global_class_col_index) continue;
            float x = (col / (float)(cols - 1)) * stretch_factor_x;
            float y = axis_inverted[col] ? (1.0f - data[row][col]) * stretch_factor_y : data[row][col] * stretch_factor_y;
            glVertex2f(x, y);
        }
        glEnd();
    }

    // Now draw the highlighted polyline
    if (hovered_row >= 0) {
        glColor3f(1.0f, 1.0f, 0.0f); // Highlight color
        glLineWidth(3.0f); // Increase line width for highlighting
        glBegin(GL_LINE_STRIP);
        for (int col = 0; col < cols; col++) {
            if (col == global_class_col_index) continue;
            float x = (col / (float)(cols - 1)) * stretch_factor_x;
            float y = axis_inverted[col] ? (1.0f - data[hovered_row][col]) * stretch_factor_y : data[hovered_row][col] * stretch_factor_y;
            glVertex2f(x, y);
        }
        glEnd();
        glLineWidth(1.0f); // Reset line width back to default
    }
}

// Function to convert window coordinates to world coordinates
void window_to_world(int x, int y, float *world_x, float *world_y) {
    // Get the size of the window
    int width = glutGet(GLUT_WINDOW_WIDTH);
    int height = glutGet(GLUT_WINDOW_HEIGHT);

    // Normalize the mouse coordinates to range [0, 1]
    *world_x = (float)x / (float)width;
    *world_y = 1.0f - (float)y / (float)height; // Invert y since window coordinates origin is top left

    // Apply the inverse of the scaling transformation
    *world_x = (*world_x - translate_x) / scale;
    *world_y = (*world_y - translate_y) / scale;

    // Apply the inverse of the stretch transformation if any
    *world_x /= stretch_factor_x;
    *world_y /= stretch_factor_y;
}

void display() {
    // Clear the color buffer
    glClearColor(0.9375f, 0.9375f, 0.9375f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up the viewport
    int width = glutGet(GLUT_WINDOW_WIDTH);
    int height = glutGet(GLUT_WINDOW_HEIGHT);
    glViewport(0, 0, width, height);

    // Set up the orthographic projection with a small margin
    float margin = 0.05f; // Margin percentage of the screen size
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = width > height ? (float)width / height : (float)height / width;
    // Apply the scale here, making sure it affects both x and y uniformly
    glOrtho(-margin * aspect * scale, (1.0f + margin) * aspect * scale, -margin * scale, (1.0f + margin) * scale, -1.0, 1.0);

    // Switch back to the modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Apply translation and scaling transformations
    glPushMatrix();
    glTranslatef(translate_x, translate_y, 0.0f);

    // Draw parallel coordinates
    if (global_data != NULL && class_info != NULL) {
        draw_parallel_coordinates(global_data, global_rows, global_cols, global_class_col_index, class_info, num_classes);
    }

    // Draw axis for each attribute
    draw_axes(global_cols);

    // Draw stars for inverted axes
    for (int col = 0; col < global_cols; col++) {
        if (col == global_class_col_index) continue; // Skip the 'class' column

        if (axis_inverted[col]) {
            float x = (col / (float)(global_cols - 1)) * stretch_factor_x;
            float y = -0.05f; // Position below the axis line
            draw_star(x, y, 0.01f); // Adjust the size as needed
        }
    }

    // Reset the line width back to default if you changed it
    glLineWidth(1.0f);

    glPopMatrix();
    glutSwapBuffers();
}

void init() {
    // Enable line smoothing
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int width = glutGet(GLUT_WINDOW_WIDTH);
        int height = glutGet(GLUT_WINDOW_HEIGHT);
        float normalized_x = (float)x / (float)width;
        float axis_space = 1.0f / (global_cols - 1);

        // Determine which axis was clicked
        for (int i = 0; i < global_cols; ++i) {
            if (i == global_class_col_index) continue; // Skip class column
            float axis_x = i * axis_space;
            if (normalized_x >= axis_x - axis_space / 2 && normalized_x < axis_x + axis_space / 2) {
                // Invert the axis
                axis_inverted[i] = !axis_inverted[i];
                glutPostRedisplay(); // Request to redraw the graph
                break;
            }
        }
    }
}

// Mouse motion callback function
void mouse_motion(int x, int y) {
    // Convert window coordinates to world coordinates
    float world_x, world_y;
    window_to_world(x, y, &world_x, &world_y);

    // Reset hovered row
    hovered_row = -1;
    float min_distance = 0.01f; // Set a threshold distance to detect hover

    // Check each polyline
    for (int row = 0; row < global_rows; row++) {
        float prev_x = 0.0f;
        float prev_y = axis_inverted[0] ? (1.0f - global_data[row][0]) * stretch_factor_y : global_data[row][0] * stretch_factor_y;

        for (int col = 1; col < global_cols; col++) {
            if (col == global_class_col_index) continue; // Skip the class column
            
            // Calculate the normalized positions of the points
            float x = (col / (float)(global_cols - 1)) * stretch_factor_x;
            float y = axis_inverted[col] ? (1.0f - global_data[row][col]) * stretch_factor_y : global_data[row][col] * stretch_factor_y;

            // Check if the mouse is close to the line segment
            if (point_to_line_dist(world_x, world_y, prev_x, prev_y, x, y) < min_distance) {
                hovered_row = row;
                break; // Stop checking if we've found the polyline being hovered
            }

            prev_x = x;
            prev_y = y;
        }
        if (hovered_row != -1) break; // Stop the loop if we've found the hovered polyline
    }
    
    // Request a redraw of the scene to update the hover highlight
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <csv_file>\n", argv[0]);
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Parallel Coordinates");
    
    init();
    
    // Set up keyboard callback
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(mouse_motion);
    
    // Load CSV data
    global_data = load_csv(argv[1], &global_rows, &global_cols, &global_class_col_index, &class_info, &num_classes);
    axis_inverted = (bool*)calloc(global_cols, sizeof(bool));

    if (global_data == NULL) {
        fprintf(stderr, "Failed to load data.\n");
        return 1;
    }

    // Normalize data
    float min_vals[global_cols], max_vals[global_cols];
    normalize_data(global_data, global_rows, global_cols, min_vals, max_vals);

    glutDisplayFunc(display);
    
    glutMainLoop();

    // Free resources
    free(axis_inverted);
    for (int i = 0; i < num_classes; i++) {
        free(class_info[i].class_name);
    }
    free(class_info);
    
    return 0;
}
