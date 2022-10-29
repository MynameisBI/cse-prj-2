#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "./headers/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./headers/stb_image_write.h"


const int max_path_length = 32;


unsigned char* uc_arrayNew_1d(int size) {
    return (unsigned char*)calloc(size, sizeof(unsigned char));
}

unsigned char* load_frames(const char* path_template, int num, int* w, int* h, int* channel) {
    // get frame dimensions to initialize array frames 
    char first_frame_path[max_path_length];
    snprintf(first_frame_path, max_path_length * sizeof(char), path_template, 0);
    stbi_load(first_frame_path, w, h, channel, 0);

    // load array frames
    unsigned char* frames = uc_arrayNew_1d(num * *w * *h * *channel);
    for (int i = 0; i < num; i++) {
        // get frame path
        char frame_path[max_path_length];
        snprintf(frame_path, max_path_length * sizeof(char), path_template, i);

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
        unsigned char* frames, int frames_num, int frame_w, int frame_h, int channel) {
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

    frames_num = 1;
    for (int i = 0; i < frames_num; i++) {
        unsigned int minSAD = -1;
        int object_position[2];

        for (int frame_y = 0; frame_y < frame_h - temp_h; frame_y++) {
            for (int frame_x = 0; frame_x < frame_w - temp_w; frame_x++) {
                unsigned int SAD = 0;
                for (int temp_y = 0; temp_y < temp_h; temp_y++) {
                    for (int temp_x = 0; temp_x < temp_w; temp_x++) {
                        int t_pixel_index = (temp_y * temp_w + temp_x) * channel;
                        int f_pixel_index = (frame_y * frame_w + frame_x) * channel + t_pixel_index;
                        SAD += get_pixels_abs_diff(&template[t_pixel_index], &frames[f_pixel_index], 3);
                    }
                }
                
                if (SAD < minSAD) {
                    minSAD = SAD;
                    object_position[0] = frame_x;
                    object_position[1] = frame_y;
                }
            }
        }

        object_positions[frames_num * i] = object_position[0];
        object_positions[frames_num * i + 1] = object_position[1];
    }

    return object_positions;
}



void draw_rectangles(unsigned char* images, int images_num, int* positions, int width, int height, int line_width) {
    //debug
    int rect_x = positions[0];
    int rect_y = positions[1];
    int rect_w = width;
    int rect_h = height;
    int lw = line_width-1;

    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 640; x++) {
            if ((((rect_x - lw <= x && x <= rect_x) || (rect_x + rect_w <= x && x <= rect_x + rect_w + lw)) && (rect_y <= y && y <= rect_y + rect_h)) ||
                    ((rect_y - lw <= y && y <= rect_y) || (rect_y + rect_h <= y && y <= rect_y + rect_h + lw)) && (rect_x <= x && x <= rect_x + rect_w)) {
                int frame_i = (y * 640 + x) * 3;
                images[frame_i] = 0;
                images[frame_i+1] = 0;
                images[frame_i+2] = 255;
            }
        }
    }
}


int main() {
    int frame_width, frame_height, template_width, template_height, channel;
    char template_path[] = "";


    unsigned char* frames = load_frames("images/frames/img%d.jpg", 63, &frame_width, &frame_height, &channel);
    unsigned char* template = stbi_load("images/template.jpg", &template_width, &template_height, &channel, 0);
    int* object_positions = track_object(template, template_width, template_height, 
            frames, 63, frame_width, frame_height, channel);

    unsigned char* frame = uc_arrayNew_1d(frame_width * frame_height * 3);
    for (int y = 0; y < frame_height; y++) {
        for (int x = 0; x < frame_width; x++) {
            for (int ch = 0; ch < channel; ch++) {
                int frame_i = (y * frame_width + x) * channel + ch;
                // frame[frame_i] = frames[4 * frame_width * frame_height * channel + frame_i];
                frame[frame_i] = frames[frame_i];
            }
        }
    }
    draw_rectangles(frame, 1, object_positions, template_width, template_height, 2);
    stbi_write_jpg("images/tracked_frames/sup.jpg", frame_width, frame_height, channel, frame, 100);

    //     //more debugging
    // unsigned char ps1[] = {3, 3, 3, 3};
    // unsigned char ps2[] = {4, 3, 3, 0};

    // unsigned int a = -1;
    // unsigned int b = 1;
    // if (a > b) printf("hello");
}