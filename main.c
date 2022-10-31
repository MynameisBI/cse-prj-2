#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./headers/stb_image_write.h"


#define MAX_PATH_LENGTH 36
#define max(a, b) ((a) > (b) ? a : b)
#define min(a, b) ((a) < (b) ? a : b)


unsigned char* uc_arrayNew_1d(int size) {
    return (unsigned char*)calloc(size, sizeof(unsigned char));
}

unsigned char* load_frames(const char* path_template, int num, int* w, int* h, int* channel) {
    // get frame dimensions to initialize array frames 
    char first_frame_path[MAX_PATH_LENGTH];
    snprintf(first_frame_path, MAX_PATH_LENGTH * sizeof(char), path_template, 0);
    stbi_load(first_frame_path, w, h, channel, 0);

    // load array frames
    unsigned char* frames = uc_arrayNew_1d(num * *w * *h * *channel);
    for (int i = 0; i < num; i++) {
        // get frame path
        char frame_path[MAX_PATH_LENGTH];
        snprintf(frame_path, MAX_PATH_LENGTH * sizeof(char), path_template, i);

        // load frame to array frames
        unsigned char* frame = stbi_load(frame_path, w, h, channel, 0);
        for (int y = 0; y < *h; y++) {
            for (int x = 0; x < *w; x++) {
                for (int ch = 0; ch < *channel; ch++) {
                    int frame_i = (y * *w + x) * *channel + ch;
                    frames[i * *w * *h * *channel + frame_i] = frame[frame_i];
                }
            }
        }
    }

    return frames;
}

unsigned int get_pixels_abs_diff(unsigned char* p1, unsigned char* p2, int channel) {
    unsigned int abs_diff = 0;
    for (int i = 0; i < channel; i++) {
        abs_diff += (unsigned int)abs(p1[i] - p2[i]);
    }
    return abs_diff;
}



int* track_object(unsigned char* base_template, int temp_w, int temp_h,
        unsigned char* frames, int frames_num, int frame_w, int frame_h, int channel, int step, float search_window_ratio, char* method) {
    int* object_positions = (int*)calloc(frames_num * 2, sizeof(int));

    // Initialize template
    unsigned char* template = uc_arrayNew_1d(temp_w * temp_h * channel);
    for (int y = 0; y < temp_h; y++) {
        for (int x = 0; x < temp_w; x++) {
            for (int ch = 0; ch < channel; ch++) {
                int index = (y * temp_w + x) * channel + ch;
                template[index] = base_template[index];
            }
        }
    }

    // Initialize search window
    int search_window_x = 0;
    int search_window_y = 0;
    int search_window_w = frame_w;
    int search_window_h = frame_h;

    // Glide template
    for (int i = 0; i < frames_num; i++) {
        unsigned int frame_index = i * frame_w * frame_h * channel;
        int object_position[2];

        if (method == "SAD") {
            unsigned int minSAD = -1;

            for (int frame_y = search_window_y; frame_y < search_window_h - temp_h; frame_y += step) {
                for (int frame_x = search_window_x; frame_x < search_window_w - temp_w; frame_x += step) {
                    unsigned int SAD = 0;
                    for (int temp_y = 0; temp_y < temp_h; temp_y += step) {
                        for (int temp_x = 0; temp_x < temp_w; temp_x += step) {
                            int t_pixel_index = (temp_y * temp_w + temp_x) * channel;
                            int f_pixel_index = ((frame_y + temp_y) * frame_w + frame_x + temp_x) * channel;
                            SAD += get_pixels_abs_diff(&template[t_pixel_index], &frames[frame_index + f_pixel_index], 3);
                        }
                    }
                    
                    if (SAD < minSAD) {
                        minSAD = SAD;
                        object_position[0] = frame_x;
                        object_position[1] = frame_y;
                    }
                }
            }
        } else if (method == "NCC") {
            double maxNCC = 0;

            for (int frame_y = search_window_y; frame_y < search_window_h - temp_h; frame_y = frame_y + step) {
                for (int frame_x = search_window_x; frame_x < search_window_w - temp_w; frame_x = frame_x + step) {
                    unsigned long s_ab = 0;
                    unsigned long s_a2 = 0;
                    unsigned long s_b2 = 0;
                    for (int temp_y = 0; temp_y < temp_h; temp_y += step) {
                        for (int temp_x = 0; temp_x < temp_w; temp_x += step) {
                            int t_pixel_index = (temp_y * temp_w + temp_x) * channel;
                            int f_pixel_index = ((frame_y + temp_y) * frame_w + frame_x + temp_x) * channel;
                            for (int ch = 0; ch < channel; ch++) {
                                unsigned int a = (unsigned int)template[t_pixel_index + ch];
                                unsigned int b = (unsigned int)frames[frame_index + f_pixel_index + ch];
                                s_ab += a * b;
                                s_a2 += pow(a, 2);
                                s_b2 += pow(b, 2);
                            }
                        }
                    }

                    double NCC = (double)s_ab / sqrt((double)s_a2 * (double)s_b2);
                    if (NCC > maxNCC) {
                        maxNCC = NCC;
                        object_position[0] = frame_x;
                        object_position[1] = frame_y;
                    }
                }
            }
        }

        object_positions[2 * i] = object_position[0];
        object_positions[2 * i + 1] = object_position[1];

        // Set new template
        for (int y = 0; y < temp_h; y++) {
            for (int x = 0; x < temp_w; x++) {
                int t_pixel_index = (y * temp_w + x) * channel;
                int f_pixel_index = ((object_position[1] + y) * frame_w + object_position[0] + x) * channel;
                for (int ch = 0; ch < channel; ch++) {
                    template[t_pixel_index + ch] = frames[frame_index + f_pixel_index + ch];
                }
            }
        }

        // Set new search window
        search_window_x = max(object_position[0] - temp_w * search_window_ratio / 2, 0);
        search_window_y = max(object_position[1] - temp_h * search_window_ratio / 2, 0);
        search_window_w = min(temp_w * search_window_ratio, frame_w - search_window_x);
        search_window_h = min(temp_h * search_window_ratio, frame_h - search_window_y);

        printf("\r%d/%d frames tracked.", i+1, frames_num);
        fflush(stdout);
    }
    printf("\n");

    return object_positions;
}

void draw_rectangles(unsigned char* frames, int frames_num, int* positions, int width, int height, int channel, int line_width) {
    int rect_w = width;
    int rect_h = height;
    int lw = line_width-1;

    for (int i = 0; i < frames_num; i++) {
        int frame_index = 640 * 480 * channel * i;

        int rect_x = positions[2 * i];
        int rect_y = positions[2 * i + 1];

        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 640; x++) {
                if ((((rect_x - lw <= x && x <= rect_x) || (rect_x + rect_w <= x && x <= rect_x + rect_w + lw)) && (rect_y <= y && y <= rect_y + rect_h)) ||
                        ((rect_y - lw <= y && y <= rect_y) || (rect_y + rect_h <= y && y <= rect_y + rect_h + lw)) && (rect_x <= x && x <= rect_x + rect_w)) {
                    int pixel_i = (y * 640 + x) * 3;
                    frames[frame_index + pixel_i] = 0;
                    frames[frame_index + pixel_i + 1] = 0;
                    frames[frame_index + pixel_i + 2] = 255;
                }
            }
        }
    }
}

void write_frames(const char* path_template, int num, int w, int h, int channel, unsigned char* frames) {
    for (int i = 0; i < num; i++) {
        char frame_path[MAX_PATH_LENGTH];
        snprintf(frame_path, MAX_PATH_LENGTH * sizeof(char), path_template, i);

        int frame_index = (w * h * channel) * i;
        stbi_write_jpg(frame_path, w, h, channel, &frames[frame_index], 100);
    }
}


int main() {
    int frame_width, frame_height, template_width, template_height, channel;
    int frames_num = 63;

    unsigned char* frames = load_frames("images/frames/img%d.jpg", frames_num, &frame_width, &frame_height, &channel);
    unsigned char* template = stbi_load("images/template.jpg", &template_width, &template_height, &channel, 0);
    printf("Load frames and template successfully. Tracking template.\n");

    int* object_positions = track_object(template, template_width, template_height, 
            frames, frames_num, frame_width, frame_height, channel, 4, 1.5, "NCC");
    printf("Track template completed. Draw rectangles.\n");

    draw_rectangles(frames, frames_num, object_positions, template_width, template_height, channel, 2);
    printf("Rectangles drawn. Writing down images.\n");

    write_frames("images/tracked_frames/img%d.jpg", frames_num, frame_width, frame_height, channel, frames);
    printf("Object tracking completed. Please check result.");

    return 0;
}