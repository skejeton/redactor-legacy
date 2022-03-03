#include <SDL2/SDL_render.h>
#include "rect.h"
#include "buffer.h"
#include "hl.c"
#include "docview.h"

static void draw_highlight(SDL_Rect viewport, SDL_Renderer *renderer, struct docview *view)
{
    struct buffer_marker marker = buffer_swap_ranges(view->doc.cursor.selection).from;
    struct buffer_range range = (struct buffer_range) { {marker.line, 0}, marker };
    char *s = buffer_get_range(&view->doc.buffer, range);
    SDL_Point size = font_measure_text(s, view->font);
    free(s);
    int x = size.x;
    int y = viewport.y+marker.line*size.y;
    s = buffer_get_range(&view->doc.buffer, view->doc.cursor.selection);

    // TODO handle variable length
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 16);
    for (int i = 0; s[i];) {
        int orig = i;
        while (s[i] && s[i] != '\n')
            i++;
        int c = s[i];
        s[i] = 0;
        int w = font_measure_text(s+orig, view->font).x;
        SDL_RenderFillRect(renderer, &(SDL_Rect){viewport.x+x, y, w, size.y});
        s[i] = c;
        if (s[i])
            i++;
        x = 0;
        y += size.y;
    }
    free(s);
}


static SDL_Point draw_lines(SDL_Rect viewport, SDL_Renderer *renderer, struct docview *view)
{
    struct buffer *buffer = &view->doc.buffer;
    SDL_Point position = (SDL_Point) { viewport.x, viewport.y };
    
    for (int i = 0; i < buffer->line_count; i++) {
        // TODO: Handle the offset more appropriately
        position.y += write_line(buffer->lines[i].data, position, view->font, renderer);
        if (position.y > viewport.h)
            break;
    }
    return position;
}


static SDL_Rect get_marker_rect(SDL_Rect viewport, struct buffer_marker marker, struct docview *view)
{
    SDL_Point viewport_pos = {viewport.x, viewport.y};
    struct buffer_range range = (struct buffer_range) { {marker.line, 0}, marker };
    char *line = buffer_get_range(&view->doc.buffer, range);
    SDL_Point line_size = font_measure_text(line, view->font);
    free(line);
    int at_line = range.from.line;
    SDL_Point cursor_position = { viewport_pos.x + line_size.x, viewport_pos.y + line_size.y * at_line };
    SDL_Rect cursor_rect = { cursor_position.x, cursor_position.y, 2, line_size.y };
    return cursor_rect;
}


static void draw_cursor(SDL_Rect viewport, SDL_Renderer *renderer, struct docview *view)
{
    SDL_Rect cursor_rect = get_marker_rect(viewport, view->doc.cursor.selection.to, view);
    SDL_Rect cursor2_rect = get_marker_rect(viewport, view->doc.cursor.selection.from, view);
    SDL_SetRenderDrawColor(renderer, 0, 150, 220, 255.0*(cos(view->blink*8)/2+0.5));
    SDL_RenderFillRect(renderer, &cursor_rect);
    SDL_SetRenderDrawColor(renderer, 220, 150, 0, 255.0*(cos(view->blink*8)/2+0.5));
    SDL_RenderFillRect(renderer, &cursor2_rect);
}


static void focus_on_cursor(SDL_Rect viewport, struct docview *view)
{
    viewport.x += view->line_column_viewport.w;
    viewport.w -= view->line_column_viewport.w;

    SDL_Rect cursor_rect = get_marker_rect(viewport, view->doc.cursor.selection.to, view);
    cursor_rect.y -= viewport.y;
    if (cursor_rect.y > (view->scroll.y+viewport.h-cursor_rect.h))
        view->scroll.y = cursor_rect.y+cursor_rect.h-viewport.h;
    if (cursor_rect.y < viewport.y+view->scroll.y)
        view->scroll.y = cursor_rect.y;
    int charw = font_measure_glyph(' ', view->font).x;

    cursor_rect.x -= viewport.x-charw;
    if (cursor_rect.x > (view->scroll.x+viewport.w-cursor_rect.w))
        view->scroll.x = cursor_rect.x+cursor_rect.w-viewport.w;
    if (cursor_rect.x-view->scroll.x-charw*2 < 0)
        view->scroll.x += cursor_rect.x-view->scroll.x-charw*2;
}

void docview_tap(bool shift, SDL_Point xy, struct docview *view)
{
    SDL_Rect viewport = view->viewport;
    rect_cut_left(&viewport, view->line_column_viewport.w);

    SDL_Point screen = {
        xy.x-viewport.x+view->scroll_damped.x,
        xy.y-viewport.y+view->scroll_damped.y,
    };
    
    SDL_Point glyph_size = font_measure_glyph(' ', view->font);
    int line = screen.y/glyph_size.y;
    // Normalize line 
    line = buffer_move_marker(&view->doc.buffer, (struct buffer_marker){line}, 0, 0).line;
    int w = 0;
    // FIXME: Hardcode!
    int mind = 100000;
    int minl = view->doc.buffer.lines[line].size;
    
    for (int i = 0; i <= view->doc.buffer.lines[line].size; ++i) {
        if (abs(w - screen.x) < mind) {
            mind = abs(w - screen.x);
            minl = i;
        }
        if (i < view->doc.buffer.lines[line].size)
            w += font_measure_glyph(view->doc.buffer.lines[line].data[i], view->font).x;
    } 
    
    docedit_set_cursor(&view->doc, shift, (struct buffer_marker){line, minl});
}

void docview_draw_lines(SDL_Rect *viewport, SDL_Renderer *renderer, struct docview *view)
{
    int last_line_no = view->doc.buffer.line_count;
    char line_no_text[32];
    snprintf(line_no_text, 32, "%d", last_line_no);
    int max_width = font_measure_text(line_no_text, view->font).x;
    SDL_Point position = {viewport->x, viewport->y};
    SDL_SetRenderDrawColor(renderer, 32, 26, 23, 255);
    SDL_SetRenderDrawColor(renderer, 250, 220, 200, 32);
    for (int i = 0; i < last_line_no; i++) {
        snprintf(line_no_text, 32, "%d", i+1);
        SDL_Point size = font_measure_text(line_no_text, view->font);
        max_width = size.x > max_width ? size.x : max_width;
        font_write_text(line_no_text, position, renderer, view->font);
        position.y += size.y;
    }

    view->line_column_viewport = rect_cut_left(viewport, max_width+font_size(view->font));
}

void docview_draw(SDL_Renderer *renderer, struct docview *view)
{
    SDL_Rect viewport = view->viewport;
    if (view->doc.cursor.selection.to.line != view->prev_cursor_pos.line ||
        view->doc.cursor.selection.to.column != view->prev_cursor_pos.column) {
        // Reset cursor blink after a movement
        view->blink = 0;
        focus_on_cursor(viewport, view);
    }

    // TODO: Use deltatime
    view->blink += 0.016;

        
    view->prev_cursor_pos = view->doc.cursor.selection.to;
    

    if (view->scroll.x < 0) 
        view->scroll.x = 0;
    if (view->scroll.y < 0) 
        view->scroll.y = 0;
    // Smooth out the scrolling
    view->scroll_damped.x += (view->scroll.x-view->scroll_damped.x)/5;
    view->scroll_damped.y += (view->scroll.y-view->scroll_damped.y)/5;

    viewport.y -= view->scroll_damped.y;

    docview_draw_lines(&viewport, renderer, view);
    viewport.y += view->scroll_damped.y;
    SDL_RenderSetClipRect(renderer, &viewport);
    viewport.y -= view->scroll_damped.y;
    viewport.x -= view->scroll_damped.x;
    SDL_Point buffer_size = draw_lines(viewport, renderer, view);    
    draw_highlight(viewport, renderer, view);

    // Clamp scrolling to not scroll outside bounds
    if (view->scroll.y > buffer_size.y-viewport.y-50) 
        view->scroll.y = buffer_size.y-viewport.y-50;
    
    // HACK: adding hardcoded offset to the cursor position to align it with the line values
    draw_cursor(viewport, renderer, view);
    
    SDL_RenderSetClipRect(renderer, NULL);
}
