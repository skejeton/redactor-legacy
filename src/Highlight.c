#include "Redactor.h"
#include "Unicode.h"

#if 0
const char *keytab[] = {
        "auto", "bool", "break", "case", "char",
        "const","continue","default","double",
        "do","else","enum","extern",
        "float","for","goto","if",
        "int","long","register","return",
        "short","signed","sizeof","static",
        "struct","switch","thread_local","typedef","union",
        "unsigned","void","volatile","while", NULL
};

static const char* FindKw(const char *kw)
{
        for (int i = 0; keytab[i]; ++i) {
                if (strncmp(kw, keytab[i], strlen(keytab[i])) == 0) {
                        return keytab[i];
                } 
        }
        return NULL;
}
#endif

enum {
        Highlight_Rule_AnyChar,
        Highlight_Rule_Wrapped,
};

struct {
        int rule_type;
        SDL_Color color;
        union {
                const char *rule_anychar;
                struct { 
                        const char *begin, *end, *slash;
                } rule_wrapped;
        };
}
typedef Highlight_Rule;




bool Highlight_Process_AnyChar(Redactor *rs, const char *charset, int *line_no, Line *line)
{
        int n = 0, c;
        char *prev = line->text;
        while ((c = Uni_Utf8_NextVeryBad((const char **)&line->text)) && Uni_Utf8_Strchr(charset, c)) {
                prev = line->text;
                n++;
        }
        line->text = prev;

        return n > 0;
}

bool Highlight_Process_Wrapped(Redactor *rs, const char *begin, const char *end, const char *slash, int *line_no, Line *line)
{
        int sb = strlen(begin), se = strlen(end), ss = strlen(slash);

        if (strncmp(line->text, begin, sb) == 0) {
                line->text += sb;
 again:
                while (*line->text && strncmp(line->text, end, se) != 0) {
                        if (strncmp(line->text, slash, ss) == 0) {
                                line->text += ss;
                        }
                        if (*line->text) {
                                line->text++;
                        }
                }
                if (*line->text) {
                        line->text += se;
                } else if (*line_no < rs->file_buffer.lines_len-1 && *end != '\n') {
                        *line = rs->file_buffer.lines[++(*line_no)];
                        goto again;
                }
                return true;
        } else {
                return false;
        }
}

void Highlight_DrawHighlightedBuffer(Redactor *rs)
{
        SDL_Point position = {rs->render_scroll.x, rs->render_scroll.y};
        int height = rs->render_font_chunks[0]->glyphs[' '].h;

        Highlight_Rule rules[32];
        int rule_count = 0;
        rules[rule_count++] = (Highlight_Rule){Highlight_Rule_AnyChar, Redactor_Color_Yellow, {"0123456789"}};
        rules[rule_count++] = (Highlight_Rule){Highlight_Rule_Wrapped, Redactor_Color_Pinkish, {.rule_wrapped = {"\"", "\"", "\\"}}};
        rules[rule_count++] = (Highlight_Rule){Highlight_Rule_Wrapped, Redactor_Color_Gray, {.rule_wrapped = {"/*", "*/", ""}}};
        rules[rule_count++] = (Highlight_Rule){Highlight_Rule_Wrapped, Redactor_Color_Gray, {.rule_wrapped = {"//", "\n", ""}}};
        

        int line_no = 0;

        while (line_no < rs->file_buffer.lines_len) {
                int col = 0;
                Line line = rs->file_buffer.lines[line_no];
                while (*line.text) {
                        int start_line_no = line_no;
                        char *start = line.text;
                        bool match = false;
                        SDL_Color color = Redactor_Color_White;

                        for (int i = 0; i < rule_count; ++i)  {
                                Highlight_Rule *rule = &rules[i];
                                switch (rule->rule_type) {
                                case Highlight_Rule_AnyChar:
                                        match = Highlight_Process_AnyChar(rs, rule->rule_anychar, &line_no, &line);
                                        break;
                                case Highlight_Rule_Wrapped:
                                        match = Highlight_Process_Wrapped(rs, rule->rule_wrapped.begin, rule->rule_wrapped.end, rule->rule_wrapped.slash, &line_no, &line);
                                        break;
                                }

                                if (match) {
                                        color = rule->color;
                                        break;
                                }
                        }

                        if (!match) {
                                line.text = start;
                                Uni_Utf8_NextVeryBad((const char **)&line.text);
                        }


                        if (start_line_no < line_no) {
                                position.y += Redactor_DrawText(rs, color, start, position.x, position.y, col).y;
                                position.x = rs->render_scroll.x;
                                start_line_no++;
                                col = 0;
                                start = rs->file_buffer.lines[line_no].text;
                        } 

                        while (start_line_no < line_no) {
                                Line cline = rs->file_buffer.lines[start_line_no];
                                position.y += Redactor_DrawText(rs, color, cline.text, position.x, position.y, col).y;
                                start_line_no++;
                        }

                        int tc = *line.text;
                        *line.text = 0;
                        SDL_Point delta = Redactor_DrawText(rs, color, start, position.x, position.y, col);
                        // FIXME: Slow, and pretty much a hack anyway
                        int c;
                        while ((c = Uni_Utf8_NextVeryBad((const char **)&start))) {
                                if (c == '\t') {
                                        col += (8 - (col % 8));
                                } else {
                                        col += 1;
                                }
                        }
                        height = delta.y;
                        position.x += delta.x;
                        *line.text = tc;
                }

                position.y += height;
                position.x = rs->render_scroll.x;
                line_no++;
        }
}

