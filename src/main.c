#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "font.h"
#include "buffer.h"
#include "docview.h"

SDL_Window *window;
SDL_Renderer *renderer;
struct docview document;
const char *font_path = "data/monospace.ttf";
struct font *font;
bool running = true;
bool is_file_new = false;
bool ctrl = 0;
bool focused = 1;
const char *filename = "tests/main.c";

static char* read_whole_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    size_t fsz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* input = (char*)malloc((fsz + 1) * sizeof(char));
    input[fsz] = 0;
    fread(input, sizeof(char), fsz, f);
    fclose(f);
    return input;
}


static void write_whole_file(const char* path, const char *contents)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return;

    fwrite(contents, strlen(contents), 1, f);
    fclose(f);
}


void loop() {
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_TEXTINPUT: {
                    if (!ctrl) {
                        buffer_write(&document.buffer, event.text.text);
                    }
                } break;
                case SDL_MOUSEWHEEL:
                    document.scroll.y -= event.wheel.y*30;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT)
                        docview_tap((SDL_Rect) {10, 10, sw, sh-40}, (SDL_Point) {event.button.x, event.button.y}, &document);
                case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        focused = 1;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        focused = 0;
                        break;
                }
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_S:
                        if (ctrl) {
                            document.buffer.dirty = 0;
                            char *file = buffer_get_whole_file(&document.buffer);
                            write_whole_file(filename, file);
                            free(file);
                        }
                        break;
                    case SDL_SCANCODE_BACKSPACE:
                        buffer_erase_character(&document.buffer);
                        break;
                    case SDL_SCANCODE_UP:
                        buffer_move(&document.buffer, 0, -1);
                        break;
                    case SDL_SCANCODE_DOWN:
                        buffer_move(&document.buffer, 0, 1);
                        break;
                    case SDL_SCANCODE_LEFT:
                        buffer_move(&document.buffer, -1, 0);
                        break;
                    case SDL_SCANCODE_RIGHT:
                        buffer_move(&document.buffer, 1, 0);
                        break;
                    case SDL_SCANCODE_RETURN:
                        buffer_write(&document.buffer, "\n");
                        break;
                    case SDL_SCANCODE_TAB:
                        for (int i = 0, col = document.buffer.cursor.column; i < (4 - col % 4); i++)
                            buffer_write(&document.buffer, " ");
                        break;
                    case SDL_SCANCODE_LCTRL:
                    case SDL_SCANCODE_RCTRL:
                        ctrl = 1;
                        break;
                    }
                    break;
                case SDL_KEYUP:
                    if (event.key.keysym.scancode == SDL_SCANCODE_LCTRL ||
                        event.key.keysym.scancode == SDL_SCANCODE_RCTRL)
                        ctrl = 0;
                    break;
                case SDL_QUIT:
                    running = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 32, 26, 23, 255);
        SDL_RenderClear(renderer);

        
        docview_draw((SDL_Rect) {10, 10, sw, sh-40}, renderer, &document);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 16);
        SDL_RenderFillRect(renderer, &(SDL_Rect){0, sh - 30, 2000, 1});
        SDL_SetRenderDrawColor(renderer, 250, 220, 200, 128);
        char txt[1024];
        snprintf(txt, 1024, "%s %s%s%d:%d fs: %d", 
            filename, is_file_new ? "(new) " : "", document.buffer.dirty ? "* " : "",
            document.buffer.cursor.line+1, document.buffer.cursor.column+1, 17 /*font_size*/);
        font_write_text(txt, (SDL_Point){0+5, sh-30+5}, renderer, font);
        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2)
        filename = argv[1];
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "could not initialize sdl ttf: %s\n", TTF_GetError());
        return 1;
    }
    
    window = SDL_CreateWindow(
        "redactor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920/2, 1080/2,
        SDL_WINDOW_SHOWN
    );
    
    if (window == NULL) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "couldn't create renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    font = document.font = font_init(font_path, 17 /*font_size*/, renderer);

    document.buffer = buffer_init();
    char *file = read_whole_file(filename);
    is_file_new = file == NULL;
    if (file != NULL) {
        // NOTE: Quick and dirty fix for extraneous line appended!
        if (*file && file[strlen(file)-1] == '\n')
            file[strlen(file)-1] = 0;

        buffer_write(&document.buffer, file);
    }


    document.buffer.cursor.column = 0;
    document.buffer.cursor.line = 0;
    document.buffer.dirty = 0;
    buffer_move(&document.buffer, 0, 0);
    free(file);
    
    loop();

    // TODO: Handle this in docview instead
    buffer_deinit(&document.buffer);
    document.buffer = (struct buffer) { 0 };

    font_deinit(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
