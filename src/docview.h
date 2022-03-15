#ifndef REDACTOR_DOCVIEW_H
#define REDACTOR_DOCVIEW_H
#include <SDL2/SDL_render.h>
#include "docedit.h"
#include "ui.h"

struct docview {
    SDL_Rect viewport;
    SDL_Rect line_column_viewport;
    struct font *font;
    struct docedit document;
    float blink;
    // Tracking the change in position of a cursor to reset the blink
    struct buffer_marker prev_cursor_pos;
    SDL_FPoint scroll;
    SDL_FPoint scroll_damped;
};

void dv_draw(struct docview *view, SDL_Renderer *renderer);
void dv_tap(struct docview *view, bool shift, SDL_Point xy);
void dv_scroll(struct docview *view, float dx, float dy);
#endif
