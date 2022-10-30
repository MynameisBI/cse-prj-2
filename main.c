#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./headers/stb_image_write.h"


#define MAX_PATH_LENGTH 36


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
        unsigned char* frames, int frames_num, int frame_w, int frame_h, int channel, int step) {
    int* object_positions = (int*)calloc(frames_num * 2, sizeof(int));

    unsigned char* template = uc_arrayNew_1d(temp_w * temp_h * channel);
    for (int y = 0; y < temp_h; y++) {
        for (int x = 0; x < temp_w; x++) {
            for (int ch = 0; ch < channel; ch++) {
                int index = (y * temp_w + x) * channel + ch;
                template[index] = base_template[index];
            }
        }
    }

    for (int i = 0; i < frames_num; i++) {
        unsigned int frame_index = i * frame_w * frame_h * channel;
        unsigned int minSAD = -1;
        int object_position[2];

        for (int frame_y = 0; frame_y < frame_h - temp_h; frame_y = frame_y + step) {
            for (int frame_x = 0; frame_x < frame_w - temp_w; frame_x = frame_x + step) {
                unsigned int SAD = 0;
                for (int temp_y = 0; temp_y < temp_h; temp_y = temp_y + step) {
                    for (int temp_x = 0; temp_x < temp_w; temp_x = temp_x + step) {
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

        object_positions[2 * i] = object_position[0];
        object_positions[2 * i + 1] = object_position[1];

        for (int y = 0; y < temp_h; y++) {
            for (int x = 0; x < temp_w; x++) {
                int t_pixel_index = (y * temp_w + x) * channel;
                int f_pixel_index = ((object_position[1] + y) * frame_w + object_position[0] + x) * channel;
                for (int ch = 0; ch < channel; ch++) {
                    template[t_pixel_index + ch] = frames[frame_index + f_pixel_index + ch];
                }
            }
        }

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

    unsigned char* frames = load_frames("images/frames/img%d.jpg", 63, &frame_width, &frame_height, &channel);
    unsigned char* template = stbi_load("images/template.jpg", &template_width, &template_height, &channel, 0);
    printf("Load frames and template successfully. Tracking template.\n");

    int* object_positions = track_object(template, template_width, template_height, 
            frames, 63, frame_width, frame_height, channel, 2);
    printf("Track template completed. Draw rectangles.\n");

    draw_rectangles(frames, 63, object_positions, template_width, template_height, channel, 2);
    printf("Rectangles drawn. Writing down images.\n");

    write_frames("images/tracked_frames/img%d.jpg", 63, frame_width, frame_height, channel, frames);
    printf("Object tracking completed. Please check result.");

    return 0;
}